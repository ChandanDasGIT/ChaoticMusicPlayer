#pragma once

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
