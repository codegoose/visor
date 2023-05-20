#include "main.rc"  // Include the resource file "main.rc"

#include <iostream>  // Include the standard input/output stream library
#include <sstream>  // Include the string stream library
#include <thread>  // Include the thread library
#include <optional>  // Include the optional library
#include <filesystem>  // Include the filesystem library

#include <windows.h>  // Include the Windows header file
#include <shellapi.h>  // Include the Windows Shell API header file
#include <shlobj.h>  // Include the Windows Shell Object header file
#include <tlhelp32.h>  // Include the Windows Tool Help header file
#include <psapi.h>  // Include the Windows Process Status API header file

#include <fmt/format.h>  // Include the fmt library for string formatting

#include "../../libs/defer.hpp"  // Include the defer library for deferred execution

static std::optional<std::filesystem::path> get_folder_path(const long &csidl) {
    CHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(0, csidl, 0, SHGFP_TYPE_CURRENT, path))) return path;  // Get the folder path for the given CSIDL identifier
    else return std::nullopt;  // Return an empty optional if the folder path retrieval fails
}

static std::optional<int> get_exe_process_id(const std::filesystem::path &exe_file_path) {
    auto snap_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);  // Create a snapshot of the current processes
    DEFER(CloseHandle(snap_handle));  // Close the handle to the snapshot automatically when the function exits
    PROCESSENTRY32 pe32;
    memset(&pe32, 0, sizeof(pe32));
    pe32.dwSize = sizeof(pe32);
    bool exe_found = false;
    if (Process32First(snap_handle, &pe32)) {  // Retrieve information about the first process in the snapshot
        while (Process32Next(snap_handle, &pe32)) {  // Iterate through the remaining processes in the snapshot
            if (auto process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID); process_handle != INVALID_HANDLE_VALUE) {  // Open a handle to the process with query and read access
                DEFER(CloseHandle(process_handle));  // Close the handle to the process automatically when the function exits
                TCHAR process_exe_path[MAX_PATH];
                if (GetModuleFileNameEx(process_handle, NULL, process_exe_path, sizeof(process_exe_path))) {  // Get the path of the executable associated with the process handle
                    if (strcmp(process_exe_path, exe_file_path.string().data()) == 0 && pe32.th32ProcessID) return pe32.th32ProcessID;  // Compare the executable path with the target path and return the process ID if they match
                }
            }   
        }
    }
    return exe_found;  // Return an empty optional if the executable process ID is not found
}

static std::optional<std::string> destroy_files(const std::filesystem::path &target) {
    std::cout << "Searching: " << target.string() << std::endl;  // Output the target path being searched
    for (auto i : std::filesystem::directory_iterator(target)) {  // Iterate through the contents of the target directory
        if (i.is_directory()) {  // If the item is a directory
            if (const auto err = destroy_files(i); err) return err;  // Recursively call the destroy_files function on the subdirectory
            for (int j = 0; j < 4; j++) {  // Attempt to delete the subdirectory up to 4 times
                if (!std::filesystem::exists(i)) break;  // If the subdirectory no longer exists, exit the loop
                std::error_code error;
                std::filesystem::remove(i, error);  // Remove the subdirectory
                if (error && j == 3) return fmt::format("Unable to delete file: {} ({})", i.path().string(), error.message());  // Return an error message if the removal fails after multiple attempts
                std::this_thread::sleep_for(std::chrono::seconds(1));  // Sleep for 1 second between deletion attempts
            }
        } else if (i.path().extension() == ".exe") {  // If the item is an executable file
            if (const auto process_id = get_exe_process_id(i.path()); process_id) {  // Get the process ID associated with the executable
                if (const auto process_handle = OpenProcess(PROCESS_TERMINATE, false, process_id.value()); process_handle) {  // Open a handle to the process with termination rights
                    DEFER(CloseHandle(process_handle));  // Close the handle to the process automatically when the function exits
                    if (!TerminateProcess(process_handle, 1)) return fmt::format("Unable to terminate process #{}: {}", process_id.value(), i.path().string());  // Terminate the process and return an error message if it fails
                }
            }
            for (int j = 0; j < 4; j++) {  // Attempt to delete the executable file up to 4 times
                if (!std::filesystem::exists(i)) break;  // If the file no longer exists, exit the loop
                std::error_code error;
                std::filesystem::remove(i, error);  // Remove the file
                if (error && j == 3) return fmt::format("Unable to delete file: {} ({})", i.path().string(), error.message());  // Return an error message if the removal fails after multiple attempts
                std::this_thread::sleep_for(std::chrono::seconds(1));  // Sleep for 1 second between deletion attempts
            }
        } else if (i.is_regular_file()) {  // If the item is a regular file
            for (int j = 0; j < 4; j++) {  // Attempt to delete the file up to 4 times
                if (!std::filesystem::exists(i)) break;  // If the file no longer exists, exit the loop
                std::error_code error;
                std::filesystem::remove(i, error);  // Remove the file
                if (error && j == 3) return fmt::format("Unable to delete file: {} ({})", i.path().string(), error.message());  // Return an error message if the removal fails after multiple attempts
                std::this_thread::sleep_for(std::chrono::seconds(1));  // Sleep for 1 second between deletion attempts
            }
        }
    }
    for (int i = 0; i < 4; i++) {  // Attempt to delete the target directory up to 4 times
        if (!std::filesystem::exists(target)) break;  // If the directory no longer exists, exit the loop
        std::error_code error;
        std::filesystem::remove(target, error);  // Remove the directory
        if (error && i == 3) return fmt::format("Unable to delete root directory: {}", target.string());  // Return an error message if the removal fails after multiple attempts
    }
    return std::nullopt;  // Return an empty optional to indicate successful deletion
}

static std::optional<std::string> commence(int arg_c, char **arg_v) {
    const auto startup_path = get_folder_path(CSIDL_STARTUP);  // Get the startup folder path
    if (!startup_path) return "Unable to get startup path.";  // Return an error message if the startup folder path is not obtained
    const auto desktop_path = get_folder_path(CSIDL_DESKTOP);  // Get the desktop folder path
    if (!desktop_path) return "Unable to get desktop path.";  // Return an error message if the desktop folder path is not obtained
    for (auto i : { startup_path, desktop_path }) {  // Iterate through the startup and desktop folder paths
        std::cout << "Searching: " << i->string() << std::endl;  // Output the folder path being searched
        for (auto j : std::filesystem::directory_iterator(i->string())) {  // Iterate through the contents of the folder
            if (j.path().extension() != ".lnk") continue;  // Skip files that are not shortcuts
            if (j.path().filename().stem().string() != "Intelligent Driver" && j.path().filename().stem().string() != "SimCoaches Background Service") continue;  // Skip shortcuts that do not match the specified names
            if (!std::filesystem::remove(j)) return fmt::format("Unable to delete shortcut: {}", j.path().string());  // Remove the shortcut and return an error message if it fails
        }
    }
    const auto programs_path = get_folder_path(CSIDL_PROGRAM_FILES);  // Get the Program Files folder path
    if (!programs_path) return "Unable to get program files path";  // Return an error message if the Program Files folder path is not obtained
    for (auto i : std::filesystem::directory_iterator(programs_path.value())) {  // Iterate through the contents of the Program Files folder
        if (!i.is_directory()) continue;  // Skip items that are not directories
        if (i.path().stem().string() != "SimCoaches") continue;  // Skip directories that do not match the specified name
        if (const auto err = destroy_files(i.path()); err) return err;  // Call the destroy_files function to delete the contents of the directory and return an error message if it fails
    }
    return std::nullopt;  // Return an empty optional to indicate successful cleanup
}

int main(int arg_c, char **arg_v) {
    DEFER(std::this_thread::sleep_for(std::chrono::seconds(1)));  // Sleep for 1 second before proceeding with the cleanup
    const auto mb_res = MessageBoxA(nullptr, "This tool will remove legacy Sim Coaches software from your PC.", "Sim Coaches Cleanup Tool", MB_OKCANCEL | MB_ICONINFORMATION);  // Show a message box to inform the user about the cleanup process
    if (mb_res == 1) {  // If the user clicks OK
        if (auto err = commence(arg_c, arg_v); err) {  // Call the commence function to perform the cleanup and check for errors
            MessageBoxA(nullptr, fmt::format("There was a problem executing the task:\n\n{}", *err).data(), "Sim Coaches Cleanup Tool", MB_OK | MB_ICONEXCLAMATION);  // Show an error message if the cleanup fails
            return 1;  // Return an error code to indicate the cleanup failure
        } else {
            MessageBoxA(nullptr, "Completed.", "Sim Coaches Cleanup Tool", MB_OK | MB_ICONINFORMATION);  // Show a completion message if the cleanup is successful
            return 0;  // Return 0 to indicate successful cleanup
        }
    } else if (mb_res == 2) {
        MessageBoxA(nullptr, "You've elected to cancel the process. No changes have been made to your PC.", "Sim Coaches Cleanup Tool", MB_OK | MB_ICONINFORMATION);  // Show a message to indicate that the user has canceled the cleanup
        return 0;  // Return 0 to indicate no changes were made
    }
}
