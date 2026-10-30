#include "backend/audioengine/decoder/decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>

#ifdef __ANDROID__
#include <android/log.h>
#include <cstdio>
#endif

namespace {

constexpr AVRational MillisecondBase{1, 1000};

void quietLogging()
{
    static std::once_flag once;
    std::call_once(once, [] {
        av_log_set_level(AV_LOG_ERROR);
#ifdef __ANDROID__
        // Android drops stderr: FFmpeg's errors go to the system log (logcat, tag "FFmpeg")
        av_log_set_callback([](void *object, int level, const char *format, va_list args) {
            if (level > av_log_get_level())
                return;
            char line[1024];
            int prefix = 1;
            av_log_format_line(object, level, format, args, line, sizeof(line), &prefix);
            __android_log_write(ANDROID_LOG_WARN, "FFmpeg", line);
        });
#endif
    });
}

// HTTPS on Android: the phone's certificates, saved by main.cpp (OpenSSL finds none itself)
void addCertificates(AVDictionary **options, const std::string &path)
{
#ifdef __ANDROID__
    if (path.rfind("https", 0) != 0)
        return;
    if (const char *certificates = std::getenv("SSL_CERT_FILE"))
        av_dict_set(options, "ca_file", certificates, 0);
#else
    (void)options;
    (void)path;
#endif
}

std::string avErrorString(int error)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error, buffer, sizeof(buffer));
    return buffer;
}

} // namespace

Decoder::Decoder()
{
    quietLogging();
}

Decoder::~Decoder()
{
    close();
}

bool Decoder::probe(const std::string &path, AudioInfo &info)
{
    quietLogging();
    AVFormatContext *format = nullptr;
    AVDictionary *options = nullptr;
    addCertificates(&options, path);
    const int opened = avformat_open_input(&format, path.c_str(), nullptr, &options);
    av_dict_free(&options);
    if (opened < 0)
        return false;

    bool ok = false;
    if (avformat_find_stream_info(format, nullptr) >= 0) {
        const int index = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (index >= 0) {
            const AVCodecParameters *par = format->streams[index]->codecpar;

            if (format->iformat && format->iformat->long_name)
                info.format = format->iformat->long_name;
            if (const AVCodecDescriptor *desc = avcodec_descriptor_get(par->codec_id)) {
                info.codec = desc->long_name ? desc->long_name : desc->name;
                info.codecName = desc->name;
                for (char &c : info.codecName)
                    c = char(std::toupper(static_cast<unsigned char>(c)));
                if (info.codecName.rfind("PCM_", 0) == 0)
                    info.codecName = "PCM";
            }

            char layout[64] = {};
            if (av_channel_layout_describe(&par->ch_layout, layout, sizeof(layout)) > 0)
                info.channelLayout = layout;

            info.bitrate = par->bit_rate > 0 ? par->bit_rate : format->bit_rate;
            info.sampleRate = par->sample_rate;
            info.channels = par->ch_layout.nb_channels;
            info.bitsPerSample = par->bits_per_raw_sample > 0 ? par->bits_per_raw_sample
                                                              : par->bits_per_coded_sample;
            if (format->duration != AV_NOPTS_VALUE)
                info.durationMs = format->duration / (AV_TIME_BASE / 1000);
            ok = true;
        }
    }

    avformat_close_input(&format);
    return ok;
}

bool Decoder::open(const std::string &path)
{
    close();
    m_error.clear();

    // Streams: reconnect on hiccups instead of ending the track; give up when stuck.
    AVDictionary *options = nullptr;
    if (path.rfind("http", 0) == 0) {
        av_dict_set(&options, "reconnect", "1", 0);
        av_dict_set(&options, "reconnect_streamed", "1", 0);
        av_dict_set(&options, "reconnect_on_network_error", "1", 0);
        av_dict_set(&options, "reconnect_delay_max", "5", 0);
        av_dict_set(&options, "rw_timeout", "15000000", 0); // microseconds
        addCertificates(&options, path);
    }
    int err = avformat_open_input(&m_format, path.c_str(), nullptr, &options);
    av_dict_free(&options);
    if (err < 0)
        return fail(path.rfind("http", 0) == 0 ? std::string("Cannot open the stream") : "Cannot open " + path, err);

    err = avformat_find_stream_info(m_format, nullptr);
    if (err < 0)
        return fail("Cannot read stream info", err);

    const AVCodec *codec = nullptr;
    m_streamIndex = av_find_best_stream(m_format, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (m_streamIndex < 0)
        return fail("No audio stream found", m_streamIndex);

    AVStream *stream = m_format->streams[m_streamIndex];
    m_startPts = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;

    m_codec = avcodec_alloc_context3(codec);
    if (!m_codec)
        return fail("Cannot allocate decoder");

    err = avcodec_parameters_to_context(m_codec, stream->codecpar);
    if (err < 0)
        return fail("Cannot configure decoder", err);
    m_codec->pkt_timebase = stream->time_base;

    err = avcodec_open2(m_codec, codec, nullptr);
    if (err < 0)
        return fail("Cannot open decoder", err);

    // Some files carry only a channel count; give them a standard layout.
    if (m_codec->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        const int channels = m_codec->ch_layout.nb_channels;
        av_channel_layout_uninit(&m_codec->ch_layout);
        av_channel_layout_default(&m_codec->ch_layout, channels);
    }

    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, OutputChannels);
    err = swr_alloc_set_opts2(&m_swr,
                              &outLayout, AV_SAMPLE_FMT_FLT, OutputRate,
                              &m_codec->ch_layout, m_codec->sample_fmt, m_codec->sample_rate,
                              0, nullptr);
    av_channel_layout_uninit(&outLayout);
    if (err < 0 || (err = swr_init(m_swr)) < 0)
        return fail("Cannot set up resampler", err);

    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    if (!m_packet || !m_frame)
        return fail("Out of memory");

    return true;
}

void Decoder::close()
{
    av_frame_free(&m_frame);
    av_packet_free(&m_packet);
    swr_free(&m_swr);
    avcodec_free_context(&m_codec);
    avformat_close_input(&m_format);

    m_streamIndex = -1;
    m_startPts = 0;
    m_skipUntilMs = -1;
    m_draining = false;
    m_finished = false;
}

bool Decoder::decode(std::vector<float> &out)
{
    if (!m_codec || m_finished)
        return false;

    for (;;) {
        int err = avcodec_receive_frame(m_codec, m_frame);

        if (err == 0) {
            const bool keep = !skipFrame(m_frame);
            if (keep)
                convert(m_frame->extended_data, m_frame->nb_samples, out);
            av_frame_unref(m_frame);
            if (keep)
                return true;
            continue;
        }

        if (err == AVERROR_EOF) {
            convert(nullptr, 0, out); // flush what the resampler still holds
            m_finished = true;
            return false;
        }

        if (err != AVERROR(EAGAIN) || m_draining) {
            if (err != AVERROR(EAGAIN))
                m_error = "Decoding failed: " + avErrorString(err);
            m_finished = true;
            return false;
        }

        // The decoder needs more input.
        err = av_read_frame(m_format, m_packet);
        if (err < 0) {
            // End of file (or read error): drain the decoder.
            m_draining = true;
            avcodec_send_packet(m_codec, nullptr);
            continue;
        }
        if (m_packet->stream_index == m_streamIndex) {
            // A corrupt packet is skipped rather than stopping playback.
            avcodec_send_packet(m_codec, m_packet);
        }
        av_packet_unref(m_packet);
    }
}

bool Decoder::seek(int64_t positionMs)
{
    if (!m_codec)
        return false;

    AVStream *stream = m_format->streams[m_streamIndex];
    const int64_t target = m_startPts + av_rescale_q(positionMs, MillisecondBase, stream->time_base);
    const int err = av_seek_frame(m_format, m_streamIndex, target, AVSEEK_FLAG_BACKWARD);
    if (err < 0) {
        m_error = "Seek failed: " + avErrorString(err);
        return false;
    }

    avcodec_flush_buffers(m_codec);
    swr_close(m_swr);
    swr_init(m_swr);

    m_skipUntilMs = positionMs;
    m_draining = false;
    m_finished = false;
    return true;
}

int64_t Decoder::durationMs() const
{
    if (!m_format)
        return 0;
    if (m_format->duration != AV_NOPTS_VALUE)
        return m_format->duration / (AV_TIME_BASE / 1000);

    const AVStream *stream = m_format->streams[m_streamIndex];
    if (stream->duration != AV_NOPTS_VALUE)
        return av_rescale_q(stream->duration, stream->time_base, MillisecondBase);
    return 0;
}

bool Decoder::fail(const std::string &message, int avError)
{
    close();
    m_error = avError < 0 ? message + ": " + avErrorString(avError) : message;
    return false;
}

// Seeking lands on the packet before the target; drop whole frames that end
// before it so playback resumes close to where the user asked.
bool Decoder::skipFrame(const AVFrame *frame)
{
    if (m_skipUntilMs < 0)
        return false;

    const int64_t pts = frame->best_effort_timestamp;
    if (pts == AV_NOPTS_VALUE || frame->sample_rate <= 0) {
        m_skipUntilMs = -1;
        return false;
    }

    const AVStream *stream = m_format->streams[m_streamIndex];
    const int64_t startMs = av_rescale_q(pts - m_startPts, stream->time_base, MillisecondBase);
    const int64_t endMs = startMs + int64_t(frame->nb_samples) * 1000 / frame->sample_rate;
    if (endMs <= m_skipUntilMs)
        return true;

    m_skipUntilMs = -1;
    return false;
}

void Decoder::convert(const uint8_t *const *input, int inputSamples, std::vector<float> &out)
{
    const int maxOut = swr_get_out_samples(m_swr, inputSamples);
    if (maxOut <= 0)
        return;

    const size_t oldSize = out.size();
    out.resize(oldSize + size_t(maxOut) * OutputChannels);
    uint8_t *dst = reinterpret_cast<uint8_t *>(out.data() + oldSize);
    // (FFmpeg 7+ takes "const uint8_t *const *", older versions "const uint8_t **")
    const int got = swr_convert(m_swr, &dst, maxOut, const_cast<const uint8_t **>(input), inputSamples);
    out.resize(oldSize + size_t(std::max(got, 0)) * OutputChannels);
}
