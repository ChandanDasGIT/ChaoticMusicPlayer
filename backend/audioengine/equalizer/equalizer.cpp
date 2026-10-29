#include "backend/audioengine/equalizer/equalizer.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double Q = 1.41;            // about one octave wide: neighbouring bands blend smoothly
constexpr double FadeSeconds = 0.02;  // a change fades in over this long
constexpr float Ceiling = 0.98f;      // the limiter keeps peaks below this (just under clipping)
constexpr double ReleaseSeconds = 0.15; // and lets go again this smoothly

} // namespace

Equalizer::Equalizer(int sampleRate, int channels)
    : m_sampleRate(sampleRate)
    , m_channels(std::clamp(channels, 1, MaxChannels))
    , m_fadeLength(std::max<size_t>(1, size_t(sampleRate * FadeSeconds)))
    , m_release(float(1 - std::exp(-1.0 / (sampleRate * ReleaseSeconds))))
{
}

// Peaking filters (Robert Bristow-Johnson's audio EQ cookbook).
Equalizer::Chain Equalizer::design(const Gains &gainsDb, bool enabled) const
{
    Chain chain;
    chain.flat = true;
    for (int i = 0; i < Bands; ++i) {
        const double gain = enabled ? std::clamp(gainsDb[size_t(i)], -MaxGainDb, MaxGainDb) : 0.0;
        if (std::abs(gain) < 0.05)
            continue; // this band stays a pass-through
        chain.flat = false;

        const double a = std::pow(10.0, gain / 40.0);
        const double w0 = 2 * Pi * Frequencies[size_t(i)] / m_sampleRate;
        const double alpha = std::sin(w0) / (2 * Q);
        const double cosW0 = std::cos(w0);
        const double a0 = 1 + alpha / a;
        Biquad &band = chain.bands[size_t(i)];
        band.b0 = (1 + alpha * a) / a0;
        band.b1 = -2 * cosW0 / a0;
        band.b2 = (1 - alpha * a) / a0;
        band.a1 = -2 * cosW0 / a0;
        band.a2 = (1 - alpha / a) / a0;
    }
    return chain;
}

void Equalizer::setGains(const Gains &gainsDb, bool enabled)
{
    Chain chain = design(gainsDb, enabled);
    std::lock_guard lock(m_mutex);
    m_pending = chain;
    m_hasPending = true;
}

float Equalizer::Chain::run(float sample, int channel)
{
    double x = double(sample);
    for (int i = 0; i < Bands; ++i) {
        const Biquad &b = bands[size_t(i)];
        std::array<double, 2> &z = state[size_t(channel)][size_t(i)];
        const double y = b.b0 * x + z[0];
        z[0] = b.b1 * x - b.a1 * y + z[1];
        z[1] = b.b2 * x - b.a2 * y;
        x = y;
    }
    return float(x);
}

void Equalizer::process(float *interleaved, size_t frames)
{
    // New settings: the old ones fade out while the new ones fade in. The new
    // filters start from the old ones' state, so nothing jumps.
    if (std::unique_lock lock(m_mutex, std::try_to_lock); lock.owns_lock() && m_hasPending) {
        if (m_current.flat && m_fadeLeft == 0)
            m_current.state = {}; // it wasn't running: no stale state to replay
        m_previous = m_current;
        m_current.bands = m_pending.bands;
        m_current.flat = m_pending.flat;
        m_hasPending = false;
        m_fadeLeft = m_fadeLength;
    }

    if (m_fadeLeft == 0 && m_current.flat) {
        m_limiterGain = 1;
        return; // off or flat: untouched
    }

    for (size_t f = 0; f < frames; ++f) {
        float *frame = interleaved + f * size_t(m_channels);
        const float mix = m_fadeLeft > 0 ? float(m_fadeLeft) / float(m_fadeLength) : 0.0f; // of the old setting
        float peak = 0;
        for (int c = 0; c < m_channels; ++c) {
            const float newer = m_current.run(frame[c], c);
            frame[c] = mix > 0 ? newer + (m_previous.run(frame[c], c) - newer) * mix : newer;
            peak = std::max(peak, std::abs(frame[c]));
        }
        limit(frame, peak);
        if (m_fadeLeft > 0)
            --m_fadeLeft;
    }
}

// Boosts keep the volume as it is; only a peak that would clip is turned down,
// at once, and the level comes back smoothly after it (both channels alike).
void Equalizer::limit(float *frame, float peak)
{
    const float target = peak > Ceiling ? Ceiling / peak : 1.0f;
    if (target < m_limiterGain)
        m_limiterGain = target;
    else
        m_limiterGain += (target - m_limiterGain) * m_release;
    if (m_limiterGain < 1.0f)
        for (int c = 0; c < m_channels; ++c)
            frame[c] *= m_limiterGain;
}
