#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// An output the user can pick. `id` is stable across restarts (PipeWire: the
// node name); an empty id means "the system default".
struct AudioDevice
{
    std::string id;
    std::string name;
};

// Plays interleaved float32 audio. Each platform has its own
// audiooutput_<platform>.cpp behind this interface (Linux: PipeWire); CMake
// picks the one for the platform being built.
//
// Samples are pulled from the audio system's realtime thread through PullFn,
// which must be fast and must not block or allocate. The stream starts
// inactive; setActive(true) starts pulling, setActive(false) pauses.
class AudioOutput
{
public:
    // Fill `dst` with up to `frames` frames; return how many were written.
    // Anything not written is filled with silence.
    using PullFn = std::function<size_t(float *dst, size_t frames)>;

    AudioOutput();
    ~AudioOutput();

    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    bool open(const std::string &name, int sampleRate, int channels, PullFn pull,
              const std::string &deviceId = {});
    void close();
    bool isOpen() const { return m_impl != nullptr; }

    void setActive(bool active);
    // Moves playback to another output ("" = system default). Keeps playing.
    void setDevice(const std::string &deviceId);

    // Outputs available right now. Blocking (briefly); call off the UI thread.
    static std::vector<AudioDevice> listDevices();

    const std::string &errorString() const { return m_error; }

private:
    struct Impl; // the platform's part (sound system objects, callbacks)

    std::unique_ptr<Impl> m_impl;
    PullFn m_pull;
    int m_sampleRate = 48000;
    int m_channels = 2;
    bool m_active = false;
    std::string m_deviceId;
    std::string m_error;
};
