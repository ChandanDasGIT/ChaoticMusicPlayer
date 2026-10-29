#include "backend/audioengine/audiooutput/audiooutput.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/dict.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace {

void ensureInitialized()
{
    static std::once_flag once;
    std::call_once(once, [] { pw_init(nullptr, nullptr); });
}

} // namespace

struct AudioOutput::Impl
{
    pw_thread_loop *loop = nullptr;
    pw_stream *stream = nullptr;
    pw_stream_events events{};

    static bool connectStream(AudioOutput &self);
    static void onProcess(void *userData);

    ~Impl()
    {
        if (loop)
            pw_thread_loop_stop(loop);
        if (stream)
            pw_stream_destroy(stream);
        if (loop)
            pw_thread_loop_destroy(loop);
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
    close();
    m_error.clear();
    ensureInitialized();

    m_pull = std::move(pull);
    m_sampleRate = sampleRate;
    m_channels = channels;
    m_deviceId = deviceId;
    m_active = false;
    m_impl = std::make_unique<Impl>();

    m_impl->loop = pw_thread_loop_new("chaotic-audio", nullptr);
    if (!m_impl->loop) {
        m_impl.reset();
        m_error = "Cannot create PipeWire thread loop";
        return false;
    }

    m_impl->events.version = PW_VERSION_STREAM_EVENTS;
    m_impl->events.process = &Impl::onProcess;

    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, name.c_str(),
        PW_KEY_NODE_NAME, name.c_str(),
        nullptr);
    if (!deviceId.empty())
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, deviceId.c_str());

    m_impl->stream = pw_stream_new_simple(pw_thread_loop_get_loop(m_impl->loop),
                                        name.c_str(), props, &m_impl->events, this);
    if (!m_impl->stream) {
        m_impl.reset();
        m_error = "Cannot create PipeWire stream";
        return false;
    }

    if (!Impl::connectStream(*this)) {
        m_impl.reset();
        m_error = "Cannot connect to PipeWire";
        return false;
    }

    if (pw_thread_loop_start(m_impl->loop) < 0) {
        m_impl.reset();
        m_error = "Cannot start PipeWire thread loop";
        return false;
    }
    return true;
}

// Connects the stream with our format; the target (if any) comes from the
// stream's properties. Call with the loop locked or before it is started.
bool AudioOutput::Impl::connectStream(AudioOutput &self)
{
    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = uint32_t(self.m_sampleRate);
    info.channels = uint32_t(self.m_channels);
    if (self.m_channels == 2) {
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
    }

    uint8_t buffer[1024];
    spa_pod_builder builder;
    spa_pod_builder_init(&builder, buffer, sizeof(buffer));
    const spa_pod *params[1] = {
        spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info),
    };

    int flags = PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS;
    if (!self.m_active)
        flags |= PW_STREAM_FLAG_INACTIVE;
    return pw_stream_connect(self.m_impl->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                             static_cast<pw_stream_flags>(flags), params, 1) >= 0;
}

void AudioOutput::close()
{
    m_impl.reset();
}

void AudioOutput::setActive(bool active)
{
    m_active = active;
    if (!m_impl)
        return;
    pw_thread_loop_lock(m_impl->loop);
    pw_stream_set_active(m_impl->stream, active);
    pw_thread_loop_unlock(m_impl->loop);
}

void AudioOutput::setDevice(const std::string &deviceId)
{
    if (deviceId == m_deviceId)
        return;
    m_deviceId = deviceId;
    if (!m_impl)
        return;

    // Reconnect with the new target; WirePlumber links us to it (or to the
    // default output when the id is empty / the device is gone).
    pw_thread_loop_lock(m_impl->loop);
    pw_stream_disconnect(m_impl->stream);
    const spa_dict_item item = SPA_DICT_ITEM_INIT(PW_KEY_TARGET_OBJECT,
                                                  deviceId.empty() ? nullptr : deviceId.c_str());
    const spa_dict dict = SPA_DICT_INIT(&item, 1);
    pw_stream_update_properties(m_impl->stream, &dict);
    if (!Impl::connectStream(*this))
        m_error = "Cannot reconnect to PipeWire";
    pw_thread_loop_unlock(m_impl->loop);
}

// Realtime thread.
void AudioOutput::Impl::onProcess(void *userData)
{
    auto *self = static_cast<AudioOutput *>(userData);
    pw_stream *stream = self->m_impl->stream;

    pw_buffer *buffer = pw_stream_dequeue_buffer(stream);
    if (!buffer)
        return;

    spa_data &data = buffer->buffer->datas[0];
    if (data.data) {
        const uint32_t stride = uint32_t(sizeof(float)) * uint32_t(self->m_channels);
        uint32_t frames = data.maxsize / stride;
        if (buffer->requested)
            frames = std::min<uint32_t>(frames, uint32_t(buffer->requested));

        auto *dst = static_cast<float *>(data.data);
        const size_t got = self->m_pull ? self->m_pull(dst, frames) : 0;
        std::fill(dst + got * self->m_channels, dst + size_t(frames) * self->m_channels, 0.0f);

        data.chunk->offset = 0;
        data.chunk->stride = int32_t(stride);
        data.chunk->size = frames * stride;
    }

    pw_stream_queue_buffer(stream, buffer);
}

// ---------------------------------------------------------------------------
// Device list: ask PipeWire's registry for every "Audio/Sink" node, then wait
// for the server to confirm it has sent everything (core sync), max 2 s.
// ---------------------------------------------------------------------------

namespace {

struct Enumeration
{
    pw_main_loop *loop = nullptr;
    pw_core *core = nullptr;
    int syncSeq = 0;
    std::vector<AudioDevice> devices;
};

void onRegistryGlobal(void *data, uint32_t, uint32_t, const char *type, uint32_t, const spa_dict *props)
{
    if (!props || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
        return;
    const char *mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!mediaClass || std::strcmp(mediaClass, "Audio/Sink") != 0)
        return;

    const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    if (!name)
        return;
    const char *description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    if (!description)
        description = spa_dict_lookup(props, PW_KEY_NODE_NICK);

    auto *state = static_cast<Enumeration *>(data);
    state->devices.push_back({name, description ? description : name});
}

void onCoreDone(void *data, uint32_t id, int seq)
{
    auto *state = static_cast<Enumeration *>(data);
    if (id == PW_ID_CORE && seq == state->syncSeq)
        pw_main_loop_quit(state->loop);
}

void onTimeout(void *data, uint64_t)
{
    pw_main_loop_quit(static_cast<Enumeration *>(data)->loop);
}

} // namespace

std::vector<AudioDevice> AudioOutput::listDevices()
{
    ensureInitialized();
    Enumeration state;

    state.loop = pw_main_loop_new(nullptr);
    if (!state.loop)
        return {};
    pw_context *context = pw_context_new(pw_main_loop_get_loop(state.loop), nullptr, 0);
    state.core = context ? pw_context_connect(context, nullptr, 0) : nullptr;

    if (state.core) {
        pw_registry *registry = pw_core_get_registry(state.core, PW_VERSION_REGISTRY, 0);

        pw_registry_events registryEvents{};
        registryEvents.version = PW_VERSION_REGISTRY_EVENTS;
        registryEvents.global = &onRegistryGlobal;
        spa_hook registryListener{};
        pw_registry_add_listener(registry, &registryListener, &registryEvents, &state);

        pw_core_events coreEvents{};
        coreEvents.version = PW_VERSION_CORE_EVENTS;
        coreEvents.done = &onCoreDone;
        spa_hook coreListener{};
        pw_core_add_listener(state.core, &coreListener, &coreEvents, &state);

        spa_source *timer = pw_loop_add_timer(pw_main_loop_get_loop(state.loop), &onTimeout, &state);
        timespec timeout{2, 0};
        pw_loop_update_timer(pw_main_loop_get_loop(state.loop), timer, &timeout, nullptr, false);

        state.syncSeq = pw_core_sync(state.core, PW_ID_CORE, 0);
        pw_main_loop_run(state.loop);

        pw_loop_destroy_source(pw_main_loop_get_loop(state.loop), timer);
        spa_hook_remove(&coreListener);
        spa_hook_remove(&registryListener);
        pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry));
        pw_core_disconnect(state.core);
    }
    if (context)
        pw_context_destroy(context);
    pw_main_loop_destroy(state.loop);

    std::sort(state.devices.begin(), state.devices.end(),
              [](const AudioDevice &a, const AudioDevice &b) { return a.name < b.name; });
    return state.devices;
}
