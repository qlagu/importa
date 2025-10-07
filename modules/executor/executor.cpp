// executor.cpp
// 提供了 executor 模块中声明的各个函数和类的具体实现。

module;

// // --- 标准库头文件 ---
// #include <codecvt>
// #include <future>
// #include <iostream>
// #include <locale>
// #include <sstream>
// #include <stdexcept>
// #include <string>
// #include <vector>

// --- 平台特定头文件 ---
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

module executor;

import std;

using namespace importa::executor;

// --- Command ---
std::string Command::to_string() const
{
    std::stringstream ss;
    ss << "\"" << executable.string() << "\"";

    for (const auto& arg : arguments)
    {
        ss << " ";
        if (arg.find(' ') != std::string::npos)
        {
            ss << "\"" << arg << "\"";
        }
        else
        {
            ss << arg;
        }
    }
    return ss.str();
}

// --- ExecutionResult ---
ExecutionResult::operator bool() const
{
    return success;
}

// --- DryRunExecutor ---
DryRunExecutor::DryRunExecutor(std::ostream& output_stream)
    : m_output_stream(output_stream)
{
}

ExecutionResult DryRunExecutor::execute(const Command& command)
{
    m_output_stream << "[DRY RUN] " << command.to_string() << std::endl;
    return { .success = true, .exit_code = 0, .std_out = "", .std_err = "" };
}

// --- LocalExecutor ---

namespace
{ // 内部辅助函数

std::string read_from_pipe(HANDLE pipe_handle)
{
    std::string output;
    const DWORD buffer_size = 4096;
    std::vector<char> buffer(buffer_size);
    DWORD bytes_read = 0;

    while (ReadFile(pipe_handle, buffer.data(), buffer_size, &bytes_read,
                    nullptr) &&
           bytes_read > 0)
    {
        output.append(buffer.data(), bytes_read);
    }
    return output;
}
} // namespace

ExecutionResult LocalExecutor::execute(const Command& command)
{
    SECURITY_ATTRIBUTES sa_attrs;
    sa_attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa_attrs.bInheritHandle = TRUE;
    sa_attrs.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read_handle = nullptr, stdout_write_handle = nullptr;
    HANDLE stderr_read_handle = nullptr, stderr_write_handle = nullptr;

    if (!CreatePipe(&stdout_read_handle, &stdout_write_handle, &sa_attrs, 0))
    {
        throw std::runtime_error(
            "LocalExecutor Error: Failed to create stdout pipe.");
    }
    if (!CreatePipe(&stderr_read_handle, &stderr_write_handle, &sa_attrs, 0))
    {
        CloseHandle(stdout_read_handle);
        CloseHandle(stdout_write_handle);
        throw std::runtime_error(
            "LocalExecutor Error: Failed to create stderr pipe.");
    }

    SetHandleInformation(stdout_read_handle, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read_handle, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION proc_info = {};
    STARTUPINFOW startup_info = {};
    startup_info.cb = sizeof(STARTUPINFOW);
    startup_info.hStdError = stderr_write_handle;
    startup_info.hStdOutput = stdout_write_handle;
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring command_line = converter.from_bytes(command.to_string());

    BOOL process_created = CreateProcessW(
        nullptr, &command_line[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr,
        command.working_directory.empty() ? nullptr
                                          : command.working_directory.c_str(),
        &startup_info, &proc_info);

    if (!process_created)
    {
        CloseHandle(stdout_read_handle);
        CloseHandle(stdout_write_handle);
        CloseHandle(stderr_read_handle);
        CloseHandle(stderr_write_handle);
        throw std::runtime_error(
            "LocalExecutor Error: CreateProcess failed. Error code: " +
            std::to_string(GetLastError()));
    }

    // 关键：父进程必须关闭管道的写入端，否则 ReadFile 会一直阻塞
    CloseHandle(stdout_write_handle);
    CloseHandle(stderr_write_handle);

    auto future_stdout =
        std::async(std::launch::async, read_from_pipe, stdout_read_handle);
    auto future_stderr =
        std::async(std::launch::async, read_from_pipe, stderr_read_handle);

    WaitForSingleObject(proc_info.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(proc_info.hProcess, &exit_code);

    std::string std_out = future_stdout.get();
    std::string std_err = future_stderr.get();

    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);
    CloseHandle(stdout_read_handle);
    CloseHandle(stderr_read_handle);

    ExecutionResult result;
    result.exit_code = static_cast<int>(exit_code);
    result.success = (exit_code == 0);
    result.std_out = std_out;
    result.std_err = std_err;
    return result;
}
