// Windows and Android: sound through miniaudio (WASAPI on Windows, AAudio on
// Android). A device's id is its name: miniaudio's own ids are binary.
#include "backend/audioengine/audiooutput/audiooutput.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include <miniaudio.h>

#include <algorithm>
#include <cstring>

namespace {

// The playback device with that name, if it is there now.
bool findDevice(ma_context &context, const std::string &name, ma_device_id &id)
{
    ma_device_info *devices = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(&context, &devices, &count, nullptr, nullptr) != MA_SUCCESS)
        return false;
    for (ma_uint32 i = 0; i < count; ++i) {
        if (name == devices[i].name) {
            id = devices[i].id;
            return true;
        }
    }
    return false;
}

} // namespace

struct AudioOutput::Impl
{
    ma_context context{};
    ma_device device{};
    bool contextReady = false;
    bool deviceReady = false;

    // Opens the device (self.m_deviceId, or the default), stopped.
    static bool openDevice(AudioOutput &self);
    static void onData(ma_device *device, void *output, const void *input, ma_uint32 frames);

    ~Impl()
    {
        if (deviceReady)
            ma_device_uninit(&device);
        if (contextReady)
            ma_context_uninit(&context);
    }
};

AudioOutput::AudioOutput() = default;

AudioOutput::~AudioOutput()
{
    close();
}

bool AudioOutput::open(const std::string &name, int sampleRate, int channels, PullFn pull,
                       const std::string &deviceId)
{
    (void)name; // (WASAPI / AAudio streams have no name to show)
    close();
    m_error.clear();

    m_pull = std::move(pull);
    m_sampleRate = sampleRate;
    m_channels = channels;
    m_deviceId = deviceId;
    m_active = false;
    m_impl = std::make_unique<Impl>();

    if (ma_context_init(nullptr, 0, nullptr, &m_impl->context) != MA_SUCCESS) {
        m_impl.reset();
        m_error = "Cannot start the sound system";
        return false;
    }
    m_impl->contextReady = true;
    if (!Impl::openDevice(*this)) {
        m_impl.reset();
        m_error = "Cannot open the sound output";
        return false;
    }
    return true;
}

bool AudioOutput::Impl::openDevice(AudioOutput &self)
{
    Impl &impl = *self.m_impl;
    ma_device_id id{};
    const bool chosen = !self.m_deviceId.empty() && findDevice(impl.context, self.m_deviceId, id);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = chosen ? &id : nullptr; // gone or none: the default
    config.playback.format = ma_format_f32;
    config.playback.channels = ma_uint32(self.m_channels);
    config.sampleRate = ma_uint32(self.m_sampleRate);
    config.dataCallback = &Impl::onData;
    config.pUserData = &self;
    config.aaudio.usage = ma_aaudio_usage_media;
    config.aaudio.contentType = ma_aaudio_content_type_music;
    if (ma_device_init(&impl.context, &config, &impl.device) != MA_SUCCESS)
        return false;
    impl.deviceReady = true;
    return true;
}

// The sound system's realtime thread.
void AudioOutput::Impl::onData(ma_device *device, void *output, const void *, ma_uint32 frames)
{
    auto *self = static_cast<AudioOutput *>(device->pUserData);
    auto *dst = static_cast<float *>(output);
    const size_t got = self->m_pull ? self->m_pull(dst, frames) : 0;
    std::fill(dst + got * size_t(self->m_channels), dst + size_t(frames) * size_t(self->m_channels), 0.0f);
}

void AudioOutput::close()
{
    m_impl.reset();
}

void AudioOutput::setActive(bool active)
{
    m_active = active;
    if (!m_impl || !m_impl->deviceReady)
        return;
    if (active)
        ma_device_start(&m_impl->device);
    else
        ma_device_stop(&m_impl->device);
}

void AudioOutput::setDevice(const std::string &deviceId)
{
    if (deviceId == m_deviceId)
        return;
    m_deviceId = deviceId;
    if (!m_impl)
        return;
    // Reopened on the new device, playing again if it was
    if (m_impl->deviceReady) {
        ma_device_uninit(&m_impl->device);
        m_impl->deviceReady = false;
    }
    if (!Impl::openDevice(*this)) {
        m_error = "Cannot open the sound output";
        return;
    }
    if (m_active)
        ma_device_start(&m_impl->device);
}

std::vector<AudioDevice> AudioOutput::listDevices()
{
    std::vector<AudioDevice> out;
    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
        return out;
    ma_device_info *devices = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(&context, &devices, &count, nullptr, nullptr) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < count; ++i)
            out.push_back({devices[i].name, devices[i].name});
    }
    ma_context_uninit(&context);
    std::sort(out.begin(), out.end(), [](const AudioDevice &a, const AudioDevice &b) { return a.name < b.name; });
    return out;
}
