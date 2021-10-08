#include <windows.h>

#include <string>
#include <optional>
#include <filesystem>
#include <codecvt>

#include <spdlog/spdlog.h>
#include <tl/expected.hpp>
#include <pystring.h>

#include "version.h"

#include "../defer.hpp"
#include "../sentry/sentry.h"

static std::vector<std::string> args;

std::string get_last_win_error() {
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0) return "No error.";
    LPSTR messageBuffer = nullptr;
    const auto size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);  
    return message;
}

static void get_command_line_arguments() {
    int num_command_line_args;
    auto command_line_args = CommandLineToArgvW(GetCommandLineW(), &num_command_line_args);
    if (!command_line_args) {
        args.clear();
        return;
    }
    args.resize(num_command_line_args);
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    for (int i = 0; i < num_command_line_args; i++) args[i] = converter.to_bytes(command_line_args[i]);
}

static tl::expected<std::filesystem::path, std::string> get_module_file_path() {
    TCHAR path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, sizeof(path)) == 0) return tl::make_unexpected(get_last_win_error());
    return path;
}

static tl::expected<int, std::string> get_return_code(const std::string_view &command_line, const std::optional<std::filesystem::path> &working_directory) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if(!CreateProcessA(NULL, (LPSTR)command_line.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return tl::make_unexpected("Unable to create process.");
    WaitForSingleObject(pi.hProcess, INFINITE);
    int result = -1;
    if (!GetExitCodeProcess(pi.hProcess,(LPDWORD)&result)) return tl::make_unexpected("Unable to get exit code.");
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return result;
}

int WinMain(HINSTANCE _instance, HINSTANCE _prev_instance, PSTR _command_line, int _command_show) {
    sc::sentry::initialize(SC_SENTRY_DSN, SC_APP_VER);
    DEFER(sc::sentry::shutdown());
    get_command_line_arguments();
    if (args.size() != 2) {
        MessageBoxA(NULL, "Unexpected number of arguments. Expecting 1 argument that represents the name of the target application.", "Issue", MB_OK | MB_ICONWARNING);
        return 2;
    }
    DEFER(spdlog::info("Execution completed."));
    if (const auto error = []() -> std::optional<std::string> {
        const auto path_res = get_module_file_path();
        if (!path_res.has_value()) return path_res.error();
        const auto command_line = fmt::format("{}\\{}.exe", path_res->parent_path().string(), args[1]);
        spdlog::info(fmt::format("Running: {}", command_line));
        const auto code_res = get_return_code(command_line, path_res->parent_path().string());
        if (!code_res) return code_res.error();
        if (*code_res != 0) return fmt::format("{}, #{}", args[1], *code_res);
        spdlog::info("Application exited without error.");
        return std::nullopt;
    }(); error) {
        spdlog::error("The target application exited in an error state: {}", *error);
        MessageBoxA(NULL, fmt::format("The program terminated in an unusual way.\n\n{}", *error).data(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    } else return 0;
}