#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct SwrContext;

// Technical properties of an audio file, as reported by FFmpeg.
struct AudioInfo
{
    std::string format;        // container, e.g. "FLAC (Free Lossless Audio Codec)"
    std::string codec;         // e.g. "MP3 (MPEG audio layer 3)"
    std::string codecName;     // short, e.g. "FLAC", "MP3", "OPUS"
    std::string channelLayout; // e.g. "stereo"
    std::string sampleFormat;  // as the decoder gives it, e.g. "s16", "fltp"
    int64_t bitrate = 0;       // bits per second
    int sampleRate = 0;
    int channels = 0;
    int bitsPerSample = 0;     // 0 for lossy formats
    int64_t durationMs = 0;
};

// Decodes an audio file with FFmpeg and converts it to interleaved
// float32 at OutputRate / OutputChannels, whatever the source format is.
class Decoder
{
public:
    static constexpr int OutputRate = 48000;
    static constexpr int OutputChannels = 2;

    Decoder();
    ~Decoder();

    // Reads the file's audio properties without decoding it.
    static bool probe(const std::string &path, AudioInfo &info);

    // What the conversion to OutputRate uses (FFmpeg's own values), for showing.
    struct ResamplerInfo
    {
        int filterSize = 0;
        double cutoff = 0; // of the lower rate's Nyquist frequency
    };
    static ResamplerInfo resamplerInfo();
    static std::string ffmpegVersion();

    Decoder(const Decoder &) = delete;
    Decoder &operator=(const Decoder &) = delete;

    bool open(const std::string &path);
    void close();
    bool isOpen() const { return m_codec != nullptr; }

    // Appends the next chunk of samples to `out`.
    // Returns false once the stream has ended (or failed); `out` may still
    // have received the final samples in that call.
    bool decode(std::vector<float> &out);

    bool seek(int64_t positionMs);
    int64_t durationMs() const;

    const std::string &errorString() const { return m_error; }

private:
    bool fail(const std::string &message, int avError = 0);
    bool skipFrame(const AVFrame *frame);
    void convert(const uint8_t *const *input, int inputSamples, std::vector<float> &out);

    AVFormatContext *m_format = nullptr;
    AVCodecContext *m_codec = nullptr;
    SwrContext *m_swr = nullptr;
    AVPacket *m_packet = nullptr;
    AVFrame *m_frame = nullptr;

    int m_streamIndex = -1;
    int64_t m_startPts = 0;      // stream start time, in stream time base
    int64_t m_skipUntilMs = -1;  // after a seek: drop frames that end before this
    bool m_draining = false;
    bool m_finished = false;
    std::string m_error;
};
