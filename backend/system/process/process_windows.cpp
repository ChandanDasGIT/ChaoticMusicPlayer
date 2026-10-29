// Windows: CreateProcess, with the output read through pipes. Each child only
// inherits its own pipe ends (several programs may start at the same time).
#include "backend/system/process/process.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <thread>

namespace {

std::wstring wide(const std::string &utf8)
{
    if (utf8.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring out(size_t(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), out.data(), size);
    return out;
}

// One argument as the C runtime reads it back (CommandLineToArgvW's rules).
void appendQuoted(std::wstring &line, const std::wstring &arg)
{
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        line += arg;
        return;
    }
    line += L'"';
    for (size_t i = 0;; ++i) {
        size_t backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') {
            ++i;
            ++backslashes;
        }
        if (i == arg.size()) {
            line.append(backslashes * 2, L'\\'); // before the closing quote
            break;
        }
        if (arg[i] == L'"')
            line.append(backslashes * 2 + 1, L'\\');
        else
            line.append(backslashes, L'\\');
        line += arg[i];
    }
    line += L'"';
}

void readAll(HANDLE pipe, std::string &into)
{
    char buffer[4096];
    DWORD got = 0;
    while (ReadFile(pipe, buffer, sizeof(buffer), &got, nullptr) && got > 0)
        into.append(buffer, got);
}

void closeHandle(HANDLE &handle)
{
    if (handle && handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);
    handle = nullptr;
}

} // namespace

ProcessResult runProcess(const std::vector<std::string> &args, int timeoutMs)
{
    ProcessResult result;
    if (args.empty())
        return result;

    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE outRead = nullptr, outWrite = nullptr, errRead = nullptr, errWrite = nullptr;
    if (!CreatePipe(&outRead, &outWrite, &inherit, 0) || !CreatePipe(&errRead, &errWrite, &inherit, 0)) {
        closeHandle(outRead);
        closeHandle(outWrite);
        return result;
    }
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);

    // Only these three handles go to the child
    HANDLE handles[] = {outWrite, errWrite, nul};
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    std::vector<char> attributeBuffer(attributeSize);
    auto *attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
    InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize);
    UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, sizeof(handles), nullptr, nullptr);

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nul;
    startup.StartupInfo.hStdOutput = outWrite;
    startup.StartupInfo.hStdError = errWrite;
    startup.lpAttributeList = attributes;

    std::wstring commandLine;
    for (const std::string &arg : args) {
        if (!commandLine.empty())
            commandLine += L' ';
        appendQuoted(commandLine, wide(arg));
    }

    // The program is looked up in PATH (".exe" added) when it isn't a path
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                        &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(attributes);
    closeHandle(outWrite);
    closeHandle(errWrite);
    closeHandle(nul);
    if (!started) {
        closeHandle(outRead);
        closeHandle(errRead);
        return result;
    }
    result.started = true;
    CloseHandle(process.hThread);

    std::thread outReader(readAll, outRead, std::ref(result.out));
    std::thread errReader(readAll, errRead, std::ref(result.err));
    if (WaitForSingleObject(process.hProcess, timeoutMs < 0 ? INFINITE : DWORD(timeoutMs)) == WAIT_TIMEOUT) {
        result.timedOut = true;
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
    }
    outReader.join(); // the pipes end with the program
    errReader.join();
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    result.exitCode = int(exitCode);
    CloseHandle(process.hProcess);
    closeHandle(outRead);
    closeHandle(errRead);
    return result;
}
