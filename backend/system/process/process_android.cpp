// Android: programs can't be started; the app's runner does the work (process.h).
#include "backend/system/process/process.h"

#include <mutex>

namespace {

std::mutex g_mutex;
ProcessRunner g_runner;

} // namespace

void setProcessRunner(ProcessRunner runner)
{
    std::lock_guard lock(g_mutex);
    g_runner = std::move(runner);
}

ProcessResult runProcess(const std::vector<std::string> &args, int timeoutMs)
{
    ProcessRunner runner;
    {
        std::lock_guard lock(g_mutex);
        runner = g_runner;
    }
    if (!runner || args.empty())
        return {};
    return runner(args, timeoutMs);
}
