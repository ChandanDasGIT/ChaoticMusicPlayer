#include "backend/audioengine/spectrum/spectrum.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float Pi = 3.14159265358979f;
constexpr float LowHz = 50.0f;
constexpr float HighHz = 16000.0f;
constexpr float FloorDb = -66.0f; // quietest level that still shows
constexpr float CeilingDb = -6.0f;  // full height
constexpr float Attack = 0.55f;   // how quickly bars rise (0..1 per update)
constexpr float Release = 0.12f;  // how quickly they fall
// Music has much less energy up high; lift it so every band moves.
constexpr float TiltDbPerOctave = 3.0f;
constexpr float TiltPivotHz = 1000.0f;

// In-place iterative radix-2 FFT.
void fft(std::vector<std::complex<float>> &data, const std::vector<std::complex<float>> &twiddles)
{
    const size_t n = data.size();
    for (size_t i = 1, j = 0; i < n; ++i) { // bit-reversal permutation
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }
    for (size_t length = 2; length <= n; length <<= 1) {
        const size_t step = n / length;
        for (size_t start = 0; start < n; start += length) {
            for (size_t k = 0; k < length / 2; ++k) {
                const std::complex<float> w = twiddles[k * step];
                const std::complex<float> even = data[start + k];
                const std::complex<float> odd = data[start + k + length / 2] * w;
                data[start + k] = even + odd;
                data[start + k + length / 2] = even - odd;
            }
        }
    }
}

} // namespace

SpectrumAnalyzer::SpectrumAnalyzer(int sampleRate)
    : m_window(FftSize)
    , m_fft(FftSize)
    , m_twiddles(FftSize / 2)
{
    for (size_t i = 0; i < FftSize; ++i) // Hann window
        m_window[i] = 0.5f - 0.5f * std::cos(2.0f * Pi * float(i) / float(FftSize - 1));
    for (size_t k = 0; k < FftSize / 2; ++k)
        m_twiddles[k] = std::polar(1.0f, -2.0f * Pi * float(k) / float(FftSize));

    // Log-spaced band edges, at least one FFT bin per band.
    const float binHz = float(sampleRate) / float(FftSize);
    size_t previous = 0;
    for (size_t b = 0; b <= BandCount; ++b) {
        const float hz = LowHz * std::pow(HighHz / LowHz, float(b) / float(BandCount));
        size_t bin = std::clamp<size_t>(size_t(std::lround(hz / binHz)), 1, FftSize / 2 - 1);
        if (b > 0 && bin <= previous)
            bin = std::min(previous + 1, FftSize / 2 - 1);
        m_bandEdges[b] = bin;
        previous = bin;
    }
    for (size_t b = 0; b < BandCount; ++b) {
        const float centerHz = LowHz * std::pow(HighHz / LowHz, (float(b) + 0.5f) / float(BandCount));
        m_tiltDb[b] = TiltDbPerOctave * std::log2(centerHz / TiltPivotHz);
    }
}

void SpectrumAnalyzer::push(const float *samples, size_t frames)
{
    if (!enabled())
        return;
    uint64_t written = m_written.load(std::memory_order_relaxed);
    for (size_t i = 0; i < frames; ++i)
        m_ring[(written + i) & (RingSize - 1)] = 0.5f * (samples[2 * i] + samples[2 * i + 1]);
    m_written.store(written + frames, std::memory_order_release);
}

bool SpectrumAnalyzer::compute(std::array<float, BandCount> &bands)
{
    const uint64_t written = m_written.load(std::memory_order_acquire);
    const bool fresh = written != m_lastWritten && written >= FftSize;
    m_lastWritten = written;

    std::array<float, BandCount> target{};
    if (fresh) {
        // The latest FftSize samples (the producer may overwrite a few while we copy:
        // harmless for a visualizer).
        for (size_t i = 0; i < FftSize; ++i)
            m_fft[i] = m_ring[(written - FftSize + i) & (RingSize - 1)] * m_window[i];
        fft(m_fft, m_twiddles);

        for (size_t b = 0; b < BandCount; ++b) {
            float peak = 0.0f;
            for (size_t bin = m_bandEdges[b]; bin < std::max(m_bandEdges[b + 1], m_bandEdges[b] + 1); ++bin)
                peak = std::max(peak, std::abs(m_fft[bin]));
            // Hann window halves the amplitude; normalize so a full-scale sine is ~0 dB.
            const float amplitude = peak * 4.0f / float(FftSize);
            const float db = 20.0f * std::log10(amplitude + 1e-9f) + m_tiltDb[b];
            target[b] = std::clamp((db - FloorDb) / (CeilingDb - FloorDb), 0.0f, 1.0f);
        }
    }

    bool moving = false;
    for (size_t b = 0; b < BandCount; ++b) {
        float &value = m_smoothed[b];
        value += (target[b] - value) * (target[b] > value ? Attack : Release);
        if (value < 0.002f)
            value = 0.0f;
        bands[b] = value;
        moving |= value > 0.0f;
    }
    return moving || fresh;
}
