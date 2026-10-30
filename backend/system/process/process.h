#pragma once

#include <functional>
#include <string>
#include <vector>

struct ProcessResult
{
    bool started = false;  // false: program not found / could not run
    bool timedOut = false; // killed after the timeout
    int exitCode = -1;
    std::string out;       // stdout
    std::string err;       // stderr

    bool ok() const { return started && !timedOut && exitCode == 0; }
};

// Runs a program (looked up in PATH) with the given arguments, without a
// shell, and collects its output. Blocking; call from worker threads.
ProcessResult runProcess(const std::vector<std::string> &args, int timeoutMs);

#ifdef __ANDROID__
// Android apps can't start programs: there, runProcess hands the call to a
// runner the app sets up (yt-dlp runs in the app's own Python; see
// wrapper/youtube/ytdlpmanager.cpp). Without one, nothing starts.
using ProcessRunner = std::function<ProcessResult(const std::vector<std::string> &args, int timeoutMs)>;
void setProcessRunner(ProcessRunner runner);
#endif
