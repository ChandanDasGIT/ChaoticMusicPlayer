#pragma once

#include <array>
#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

// Frequency analysis for the visualizer.
//
// push() is called from the realtime audio thread with what is about to be
// played: it only copies a mono mix into a lock-free buffer. compute() runs
// elsewhere (UI timer): FFT of the latest samples -> `BandCount` bands from
// 50 Hz to 16 kHz, log-spaced, each 0..1, rising fast and falling slowly.
// When no new audio arrives (paused) the bands fall back to 0.
class SpectrumAnalyzer
{
public:
    static constexpr size_t BandCount = 32;
    static constexpr size_t FftSize = 1024;

    explicit SpectrumAnalyzer(int sampleRate);

    void setEnabled(bool enabled) { m_enabled.store(enabled, std::memory_order_relaxed); }
    bool enabled() const { return m_enabled.load(std::memory_order_relaxed); }

    // Realtime thread. Interleaved stereo.
    void push(const float *samples, size_t frames);

    // Returns false once everything has settled at 0 (nothing more to draw).
    bool compute(std::array<float, BandCount> &bands);

private:
    static constexpr size_t RingSize = 4096; // power of two, > FftSize

    std::atomic<bool> m_enabled{false};
    std::array<float, RingSize> m_ring{};
    std::atomic<uint64_t> m_written{0};

    // compute() state
    uint64_t m_lastWritten = 0;
    std::vector<float> m_window;
    std::vector<std::complex<float>> m_fft;
    std::vector<std::complex<float>> m_twiddles;
    std::array<size_t, BandCount + 1> m_bandEdges{}; // FFT bins
    std::array<float, BandCount> m_tiltDb{};
    std::array<float, BandCount> m_smoothed{};
};
