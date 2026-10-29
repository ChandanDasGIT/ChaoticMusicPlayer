// POSIX (Linux, Android): fork / exec, with the output read through pipes.
#include "backend/system/process/process.h"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace {

void closeFd(int &fd)
{
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

} // namespace

ProcessResult runProcess(const std::vector<std::string> &args, int timeoutMs)
{
    ProcessResult result;
    if (args.empty())
        return result;

    int outPipe[2] = {-1, -1};
    int errPipe[2] = {-1, -1};
    if (pipe2(outPipe, O_CLOEXEC) != 0 || pipe2(errPipe, O_CLOEXEC) != 0) {
        closeFd(outPipe[0]);
        closeFd(outPipe[1]);
        return result;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, errPipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);

    std::vector<char *> argv;
    for (const std::string &arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);

    pid_t pid = -1;
    const int spawned = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    closeFd(outPipe[1]);
    closeFd(errPipe[1]);
    if (spawned != 0) {
        closeFd(outPipe[0]);
        closeFd(errPipe[0]);
        return result;
    }
    result.started = true;

    // Read both pipes until they close or the time is up.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    pollfd fds[2] = {{outPipe[0], POLLIN, 0}, {errPipe[0], POLLIN, 0}};
    char buffer[8192];
    while (fds[0].fd >= 0 || fds[1].fd >= 0) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                              deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            result.timedOut = true;
            kill(pid, SIGKILL);
            break;
        }
        if (poll(fds, 2, int(left)) < 0)
            break;
        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0 || !(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;
            const ssize_t n = ::read(fds[i].fd, buffer, sizeof(buffer));
            if (n > 0) {
                (i == 0 ? result.out : result.err).append(buffer, size_t(n));
            } else {
                ::close(fds[i].fd);
                fds[i].fd = -1;
            }
        }
    }
    closeFd(outPipe[0]);
    closeFd(errPipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        result.exitCode = WEXITSTATUS(status);
    return result;
}
