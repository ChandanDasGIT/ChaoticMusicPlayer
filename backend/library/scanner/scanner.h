#pragma once

#include "backend/library/track/track.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Finds audio files in folders (recursively) and reads their metadata on a
// worker thread. Requests are queued; callbacks run on the worker thread.
class Scanner
{
public:
    struct Callbacks
    {
        std::function<void(size_t done, size_t total)> progress;
        std::function<void(std::vector<Track> tracks)> tracksFound; // delivered in batches
        std::function<void()> finished; // once per scan() request
    };

    explicit Scanner(Callbacks callbacks);
    ~Scanner();

    Scanner(const Scanner &) = delete;
    Scanner &operator=(const Scanner &) = delete;

    // Folders and/or individual files.
    void scan(std::vector<std::string> paths);

    static bool isAudioFile(const std::filesystem::path &path);

private:
    void run();
    void process(const std::vector<std::string> &paths);
    std::vector<std::filesystem::path> collect(const std::vector<std::string> &paths) const;
    bool quitRequested();

    const Callbacks m_callbacks;

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<std::vector<std::string>> m_jobs;
    bool m_quit = false;

    std::thread m_thread; // last member: starts after everything above exists
};
