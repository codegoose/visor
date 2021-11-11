#include "main.rc"

#include <iostream>
#include <sstream>
#include <thread>
#include <optional>
#include <filesystem>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <fmt/format.h>

#include "../../libs/defer.hpp"

static std::optional<std::filesystem::path> get_folder_path(const long &csidl) {
    CHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(0, csidl, 0, SHGFP_TYPE_CURRENT, path))) return path;
    else return std::nullopt;
}

static std::optional<int> get_exe_process_id(const std::filesystem::path &exe_file_path) {
    auto snap_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DEFER(CloseHandle(snap_handle));
    PROCESSENTRY32 pe32;
    memset(&pe32, 0, sizeof(pe32));
    pe32.dwSize = sizeof(pe32);
    bool exe_found = false;
    if (Process32First(snap_handle, &pe32)) {
        while (Process32Next(snap_handle, &pe32)) {
            if (auto process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID); process_handle != INVALID_HANDLE_VALUE) {
                DEFER(CloseHandle(process_handle));
                TCHAR process_exe_path[MAX_PATH];
                if (GetModuleFileNameEx(process_handle, NULL, process_exe_path, sizeof(process_exe_path))) {
                    if (strcmp(process_exe_path, exe_file_path.string().data()) == 0 && pe32.th32ProcessID) return pe32.th32ProcessID;
                }
            }   
        }
    }
    return exe_found;
}

static std::optional<std::string> destroy_files(const std::filesystem::path &target) {
    std::cout << "Searching: " << target.string() << std::endl;
    for (auto i : std::filesystem::directory_iterator(target)) {
        if (i.is_directory()) {
            if (const auto err = destroy_files(i); err) return err;
            for (int j = 0; j < 4; j++) {
                if (!std::filesystem::exists(i)) break;
                std::error_code error;
                std::filesystem::remove(i, error);
                if (error && j == 3) return fmt::format("Unable to delete file: {} ({})", i.path().string(), error.message());
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } else if (i.path().extension() == ".exe") {
            if (const auto process_id = get_exe_process_id(i.path()); process_id) {
                if (const auto process_handle = OpenProcess(PROCESS_TERMINATE, false, process_id.value()); process_handle) {
                    DEFER(CloseHandle(process_handle));
                    if (!TerminateProcess(process_handle, 1)) return fmt::format("Unable to terminate process #{}: {}", process_id.value(), i.path().string());
                }
            }
            for (int j = 0; j < 4; j++) {
                if (!std::filesystem::exists(i)) break;
                std::error_code error;
                std::filesystem::remove(i, error);
                if (error && j == 3) return fmt::format("Unable to delete file: {} ({})", i.path().string(), error.message());
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } else if (i.is_regular_file()) {
            for (int j = 0; j < 4; j++) {
                if (!std::filesystem::exists(i)) break;
                std::error_code error;
                std::filesystem::remove(i, error);
                if (error && j == 3) return fmt::format("Unable to delete file: {} ({})", i.path().string(), error.message());
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        if (!std::filesystem::exists(target)) break;
        std::error_code error;
        std::filesystem::remove(target, error);
        if (error && i == 3) return fmt::format("Unable to delete root directory: {}", target.string());
    }
    return std::nullopt;
}

static std::optional<std::string> commence(int arg_c, char **arg_v) {
    const auto startup_path = get_folder_path(CSIDL_STARTUP);
    if (!startup_path) return "Unable to get startup path.";
    const auto desktop_path = get_folder_path(CSIDL_DESKTOP);
    if (!desktop_path) return "Unable to get desktop path.";
    for (auto i : { startup_path, desktop_path }) {
        std::cout << "Searching: " << i->string() << std::endl;
        for (auto j : std::filesystem::directory_iterator(i->string())) {
            if (j.path().extension() != ".lnk") continue;
            if (j.path().filename().stem().string() != "Intelligent Driver" && j.path().filename().stem().string() != "SimCoaches Background Service") continue;
            if (!std::filesystem::remove(j)) return fmt::format("Unable to delete shortcut: {}", j.path().string());
        }
    }
    const auto programs_path = get_folder_path(CSIDL_PROGRAM_FILES);
    if (!programs_path) return "Unable to get program files path";
    for (auto i : std::filesystem::directory_iterator(programs_path.value())) {
        if (!i.is_directory()) continue;
        if (i.path().stem().string() != "SimCoaches") continue;
        if (const auto err = destroy_files(i.path()); err) return err;
    }
    return std::nullopt;
}

int main(int arg_c, char **arg_v) {
    DEFER(std::this_thread::sleep_for(std::chrono::seconds(1)));
    const auto mb_res = MessageBoxA(nullptr, "This tool will remove legacy Sim Coaches software from your PC.", "Sim Coaches Cleanup Tool", MB_OKCANCEL | MB_ICONINFORMATION);
    if (mb_res == 1) {
        if (auto err = commence(arg_c, arg_v); err) {
            MessageBoxA(nullptr, fmt::format("There was a problem executing the task:\n\n{}", *err).data(), "Sim Coaches Cleanup Tool", MB_OK | MB_ICONEXCLAMATION);
            return 1;
        } else {
            MessageBoxA(nullptr, "Completed.", "Sim Coaches Cleanup Tool", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
    } else if (mb_res == 2) {
        MessageBoxA(nullptr, "You've elected to cancel the process. No changes have been made to your PC.", "Sim Coaches Cleanup Tool", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
}
