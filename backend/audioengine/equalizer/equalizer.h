#pragma once

#include <array>
#include <cstddef>
#include <mutex>

// 10-band graphic equalizer for interleaved float audio: one peaking filter
// per octave band (31 Hz ... 16 kHz, ±12 dB). Boosts don't lower the overall
// volume; a limiter only turns down the moments that would clip. Off or flat,
// the audio is left untouched.
//
// setGains() may be called from any thread (e.g. while a slider is dragged);
// process() runs on the realtime audio thread and never waits for it: new
// settings are picked up on its next call, faded in over a few milliseconds
// so changes never click.
class Equalizer
{
public:
    static constexpr int Bands = 10;
    static constexpr std::array<double, Bands> Frequencies = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    static constexpr double MaxGainDb = 12;

    using Gains = std::array<double, Bands>; // dB per band

    Equalizer(int sampleRate, int channels);

    void setGains(const Gains &gainsDb, bool enabled);

    // Realtime thread.
    void process(float *interleaved, size_t frames);

private:
    static constexpr int MaxChannels = 2;

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };
    // A full filter setting, with each band's state per channel (transposed direct form II).
    struct Chain
    {
        std::array<Biquad, Bands> bands;
        bool flat = true; // passes audio through unchanged
        std::array<std::array<std::array<double, 2>, Bands>, MaxChannels> state{};

        float run(float sample, int channel);
    };

    Chain design(const Gains &gainsDb, bool enabled) const;
    void limit(float *frame, float peak);

    const int m_sampleRate;
    const int m_channels;

    // Hand-over from setGains() (guarded; process() only ever try-locks)
    std::mutex m_mutex;
    Chain m_pending;
    bool m_hasPending = false;

    // Realtime thread
    Chain m_current;
    Chain m_previous;       // faded out after a change
    size_t m_fadeLeft = 0;  // frames of fade still to go
    size_t m_fadeLength;
    const float m_release;     // limiter: how fast the level comes back, per frame
    float m_limiterGain = 1;
};
