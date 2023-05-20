#include "iracing.h"  // Include the header file "iracing.h"

#include <vector>  // Include the header file for using std::vector
#include <mutex>  // Include the header file for using std::mutex
#include <thread>  // Include the header file for using std::thread
#include <atomic>  // Include the header file for using std::atomic
#include <optional>  // Include the header file for using std::optional
#include <chrono>  // Include the header file for using std::chrono
#include <algorithm>  // Include the header file for using std::algorithm
#include <map>  // Include the header file for using std::map
#include <string>  // Include the header file for using std::string

#include <spdlog/spdlog.h>  // Include the header file for using spdlog
#include <spdlog/fmt/bin_to_hex.h>  // Include the header file for using spdlog hex formatting

#include <windows.h>  // Include the Windows header file

#include "../defer.hpp"  // Include the custom header file "defer.hpp"

namespace sc::iracing {  // Start of the sc::iracing namespace

    static const auto mapped_file_name = "Local\\IRSDKMemMapFileName";  // Declaration of a static constant variable "mapped_file_name"
    static const size_t mapped_file_length = 1164 * 1024;  // Declaration of a static constant variable "mapped_file_length"
    static const auto data_event_name = "Local\\IRSDKDataValidEvent";  // Declaration of a static constant variable "data_event_name"
    static const auto broadcast_message_name = "IRSDK_BROADCASTMSG";  // Declaration of a static constant variable "broadcast_message_name"

    static std::thread worker;  // Declaration of a static std::thread variable "worker"
    static std::atomic_bool working = false;  // Declaration of a static std::atomic_bool variable "working"

    static std::atomic<status> current_status = status::stopped;  // Declaration of a static std::atomic<status> variable "current_status"

    std::mutex tele_members_mutex;  // Declaration of a std::mutex variable "tele_members_mutex"
    std::map<std::string, int> tele_members;  // Declaration of a std::map<std::string, int> variable "tele_members"

    std::atomic<bool> tele_prev_valid = false;  // Declaration of a std::atomic<bool> variable "tele_prev_valid"
    std::atomic<float> tele_lap_percent = 0;  // Declaration of a std::atomic<float> variable "tele_lap_percent"
    std::atomic<float> tele_rpm = 0, tele_rpm_prev = 0;  // Declaration of std::atomic<float> variables "tele_rpm" and "tele_rpm_prev"
    std::atomic<float> tele_speed = 0, tele_speed_prev = 0;  // Declaration of std::atomic<float> variables "tele_speed" and "tele_speed_prev"
    std::atomic<int> tele_gear = 0, tele_gear_prev = 0;  // Declaration of std::atomic<int> variables "tele_gear" and "tele_gear_prev"

    struct moment {  // Definition of a struct named "moment"

        bool filled = false;  // Declaration and initialization of a bool member variable "filled"
        float rpm, speed;  // Declaration of float member variables "rpm" and "speed"
        int gear;  // Declaration of an int member variable "gear"
    };

    std::vector<moment> moments;  // Declaration of a std::vector<moment> variable "moments"

    struct variable_buffer_header {  // Definition of a struct named "variable_buffer_header"

        int32_t tick_count;  // Declaration of an int32_t member variable "tick_count"
        int32_t data_offset;  // Declaration of an int32_t member variable "data_offset"
        uint32_t _padding_1[2];  // Declaration of a uint32_t array member variable "_padding_1"
    };

    struct header {  // Definition of a struct named "header"

        int32_t version;  // Declaration of an int32_t member variable "version"
        int32_t status;  // Declaration of an int32_t member variable "status"
        int32_t tick_rate;  // Declaration of an int32_t member variable "tick_rate"
        int32_t session_info_update;  // Declaration of an int32_t member variable "session_info_update"
        int32_t session_info_length;  // Declaration of an int32_t member variable "session_info_length"
        int32_t session_info_offset;  // Declaration of an int32_t member variable "session_info_offset"
        int32_t num_variables;  // Declaration of an int32_t member variable "num_variables"
        int32_t variables_header_offset;  // Declaration of an int32_t member variable "variables_header_offset"
        int32_t num_buffers;  // Declaration of an int32_t member variable "num_buffers"
        int32_t buffer_length;  // Declaration of an int32_t member variable "buffer_length"
        uint32_t _padding_1[2];  // Declaration of a uint32_t array member variable "_padding_1"
        variable_buffer_header buffers[4];  // Declaration of an array of variable_buffer_header named "buffers"
    };

    struct variable_header {  // Definition of a struct named "variable_header"

        int32_t type;  // Declaration of an int32_t member variable "type"
        int32_t offset;  // Declaration of an int32_t member variable "offset"
        int32_t count;  // Declaration of an int32_t member variable "count"
        std::byte count_as_time;  // Declaration of a std::byte member variable "count_as_time"
        uint8_t _padding_1[3];  // Declaration of a uint8_t array member variable "_padding_1"
        std::byte name[32];  // Declaration of a std::byte array member variable "name"
        std::byte description[64];  // Declaration of a std::byte array member variable "description"
        std::byte unit[32];  // Declaration of a std::byte array member variable "unit"
    };

    static variable_buffer_header *find_recent_valid_buffer(header *telemetry_header) {  // Definition of a static function "find_recent_valid_buffer" that returns a variable_buffer_header pointer and takes a header pointer "telemetry_header" as a parameter
        std::vector<variable_buffer_header *> sorted_buffers;  // Declaration of a std::vector<variable_buffer_header *> variable "sorted_buffers"
        for (int i = 0; i < telemetry_header->num_buffers; i++) sorted_buffers.push_back(&telemetry_header->buffers[i]);  // Iterate through the buffers and add their addresses to the "sorted_buffers" vector
        std::sort(sorted_buffers.begin(), sorted_buffers.end(), [](variable_buffer_header *first, variable_buffer_header *second) {  // Sort the "sorted_buffers" vector based on the tick_count of the buffers in descending order
            return first->tick_count > second->tick_count;
        });
        return sorted_buffers[1];  // Return the second element (index 1) of the sorted_buffers vector
    }

    static void process_lap_progress() {  // Definition of a static function "process_lap_progress" that doesn't return anything and takes no parameters
        const auto i = static_cast<size_t>(tele_lap_percent * static_cast<float>(moments.size()));  // Calculate the index i based on the tele_lap_percent and the size of the moments vector
        if (i < 0 || i >= moments.size()) return;  // If the index i is out of range, return
        size_t nearest = -1;  // Declaration of a size_t variable "nearest" initialized with -1
        for (auto j = i + 1; j < moments.size() && j < i + 1000; j++) {  // Iterate through the moments vector starting from i+1 to i+1000
            if (moments[j].filled) {  // If the moment at index j is filled
                nearest = j;  // Set the nearest variable to j
                break;  // Exit the loop
            }
        }
        if (nearest >= 0 && nearest < moments.size()) {  // If the nearest variable is within range
            tele_prev_valid = true;  // Set tele_prev_valid to true
            tele_rpm_prev = moments[nearest].rpm;  // Set tele_rpm_prev to the rpm value of the nearest moment
            tele_speed_prev = moments[nearest].speed;  // Set tele_speed_prev to the speed value of the nearest moment
            tele_gear_prev = moments[nearest].gear;  // Set tele_gear_prev to the gear value of the nearest moment
        }
        else {  // If no valid nearest moment is found
            tele_prev_valid = false;  // Set tele_prev_valid to false
            const auto now = std::chrono::system_clock::now();  // Get the current time
            static auto last = now;  // Declaration of a static auto variable "last" initialized with now
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last).count() > 2) {  // If more than 2 seconds have passed since the last log
                spdlog::info("No data ahead: {}", i);  // Output a log message indicating no data ahead at index i
                last = now;  // Update the last log time to now
            }
        }
        if (tele_speed > 5) {  // If tele_speed is greater than 5
            moments[i] = {  // Assign a new moment at index i with the following values
                true,
                tele_rpm,
                tele_speed,
                tele_gear
            };
        }
    }

    static void process_telemetry(header *telemetry_header, variable_header *variables) {  // Definition of a static function "process_telemetry" that doesn't return anything and takes a header pointer "telemetry_header" and a variable_header pointer "variables" as parameters
        auto variable_buffer = find_recent_valid_buffer(telemetry_header);  // Get the recent valid buffer using the find_recent_valid_buffer function
        for (int i = 0; i < telemetry_header->num_variables; i++) {  // Iterate through the variables
            if (strcmp("RPM", reinterpret_cast<char *>(variables[i].name)) == 0) {  // If the variable name is "RPM"
                auto value = reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(telemetry_header) + variable_buffer->data_offset + variables[i].offset);  // Get the value of the RPM variable
                spdlog::info("RPM: {}, {}", *value, variables[i].type);  // Output a log message with the RPM value and type
            } else if (strcmp("LapDistPct", reinterpret_cast<char *>(variables[i].name)) == 0) {  // If the variable name is "LapDistPct"
                tele_lap_percent = *reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(telemetry_header) + variable_buffer->data_offset + variables[i].offset);  // Get the value of the lap percentage
            } else if (strcmp("Speed", reinterpret_cast<char *>(variables[i].name)) == 0) {  // If the variable name is "Speed"
                tele_speed = *reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(telemetry_header) + variable_buffer->data_offset + variables[i].offset);  // Get the value of the speed variable
                tele_speed = tele_speed * 2.2f;  // Convert the speed from m/s to mph
            } else if (strcmp("Gear", reinterpret_cast<char *>(variables[i].name)) == 0) {  // If the variable name is "Gear"
                tele_gear = *reinterpret_cast<int *>(reinterpret_cast<uintptr_t>(telemetry_header) + variable_buffer->data_offset + variables[i].offset);  // Get the value of the gear variable
            } else if (strcmp("LFshockDefl", reinterpret_cast<char *>(variables[i].name)) == 0) {  // If the variable name is "LFshockDefl"
                // spdlog::info("LFshockDefl: {}", *reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(telemetry_header) + variable_buffer->data_offset + variables[i].offset));
            } else if (strcmp("LFshockVel_ST", reinterpret_cast<char *>(variables[i].name)) == 0) {  // If the variable name is "LFshockVel_ST"
                spdlog::info("LFshockVel_ST: {}", *reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(telemetry_header) + variable_buffer->data_offset + variables[i].offset));  // Output a log message with the LFshockVel_ST value
            }
        }
    }

    static void work() {  // Definition of a static function "work" that doesn't return anything and takes no parameters
        DEFER(
            current_status = status::stopped;  // Set current_status to status::stopped when exiting the function
        );
        std::optional<std::chrono::system_clock::time_point> last_file_handle_open_attempt;  // Declaration of an std::optional<std::chrono::system_clock::time_point> variable "last_file_handle_open_attempt"
        for (;;) {  // Infinite loop
            current_status = status::searching;  // Set current_status to status::searching
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Sleep for 100 milliseconds
            if (!working) return;  // If working is false, return from the function
            if (!last_file_handle_open_attempt || std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - *last_file_handle_open_attempt).count() > 1) {  // If there is no last_file_handle_open_attempt or more than 1 second has passed since the last attempt
                std::optional<HANDLE> file_handle;;  // Declaration of an std::optional<HANDLE> variable "file_handle"
                std::optional<std::byte *> mapped_file_buffer;  // Declaration of an std::optional<std::byte *> variable "mapped_file_buffer"
                std::optional<HANDLE> event_handle;  // Declaration of an std::optional<HANDLE> variable "event_handle"
                DEFER(
                    if (event_handle) {
                        CloseHandle(*event_handle);  // Close the event handle if it exists
                        spdlog::debug("Closed iRacing event handle.");
                        event_handle.reset();
                    }
                    if (mapped_file_buffer) {
                        UnmapViewOfFile(*mapped_file_buffer);  // Unmap the mapped file buffer if it exists
                        spdlog::debug("Unmapped iRacing memory file.");
                        mapped_file_buffer.reset();
                    }
                    if (file_handle) {
                        CloseHandle(*file_handle);  // Close the file handle if it exists
                        spdlog::debug("Closed iRacing memory file handle.");
                        file_handle.reset();
                    }
                    last_file_handle_open_attempt = std::chrono::system_clock::now();  // Set last_file_handle_open_attempt to the current time
                );
                if (const auto attempted_handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapped_file_name); attempted_handle) {  // Try to open the file mapping with the specified name and get the handle
                    file_handle = attempted_handle;  // Assign the handle to the file_handle variable
                    if (const auto attempted_mapped_buffer = MapViewOfFile(*file_handle, FILE_MAP_ALL_ACCESS, 0, 0, mapped_file_length); attempted_mapped_buffer) {  // Try to map the file view with the specified parameters and get the mapped buffer
                        mapped_file_buffer = reinterpret_cast<std::byte *>(attempted_mapped_buffer);  // Assign the mapped buffer to the mapped_file_buffer variable
                        if (auto attempted_event_handle = OpenEventA(SYNCHRONIZE, FALSE, data_event_name); attempted_event_handle) {  // Try to open the event handle with the specified parameters and get the handle
                            event_handle = attempted_event_handle;  // Assign the handle to the event_handle variable
                            spdlog::debug("iRacing telemetry is online.");
                            while (working) {  // Loop while working is true
                                const auto wait_res = WaitForSingleObject(*event_handle, 1000);  // Wait for the event handle with a timeout of 1000 milliseconds
                                if (wait_res == WAIT_OBJECT_0) {  // If the event is signaled
                                    current_status = status::live;  // Set current_status to status::live
                                    process_telemetry(
                                        reinterpret_cast<header *>(*mapped_file_buffer),  // Cast the mapped file buffer to a header pointer
                                        reinterpret_cast<variable_header *>(reinterpret_cast<uintptr_t>(*mapped_file_buffer) + reinterpret_cast<header *>(*mapped_file_buffer)->variables_header_offset)  // Calculate the variable header pointer using the offset in the header
                                    );
                                    process_lap_progress();
                                } else if (wait_res == WAIT_TIMEOUT) {  // If the wait timed out
                                    current_status = status::connected;  // Set current_status to status::connected
                                } else if (wait_res == WAIT_ABANDONED) {  // If the wait was abandoned
                                    spdlog::warn("iRacing synchronization handle was abandoned.");
                                    break;  // Exit the loop
                                }
                                else if (wait_res == WAIT_FAILED) {  // If the wait failed
                                    spdlog::warn("iRacing synchronization handle returned fail state.");
                                    break;  // Exit the loop
                                }
                            }
                        } else spdlog::warn("Unable to open iRacing synchronization handle.");
                    } else spdlog::warn("Unable to map iRacing memory file.");
                }
            }
        }
    }
}

void sc::iracing::startup() {  // Definition of a function "startup" in the sc::iracing namespace
    shutdown();  // Call the shutdown function
    moments.resize(100000);  // Resize the moments vector to hold 100000 elements
    spdlog::debug("Starting up iRacing telemetry worker.");
    working = true;  // Set working to true
    worker = std::thread(work);  // Create a thread "worker" and assign it to the work function
}

void sc::iracing::shutdown() {  // Definition of a function "shutdown" in the sc::iracing namespace
    working = false;  // Set working to false
    if (worker.joinable()) {  // If the worker thread is joinable
        worker.join();  // Join the worker thread
        spdlog::debug("Shutdown iRacing telemetry worker.");
    }
    moments.clear();  // Clear the moments vector
}

const sc::iracing::status &sc::iracing::get_status() {  // Definition of a function "get_status" in the sc::iracing namespace that returns a const reference to status
    return current_status;  // Return the current_status
}

const std::atomic<bool> &sc::iracing::prev() {  // Definition of a function "prev" in the sc::iracing namespace that returns a const reference to std::atomic<bool>
    return tele_prev_valid;  // Return tele_prev_valid
}

const std::atomic<float> &sc::iracing::lap_percent() {  // Definition of a function "lap_percent" in the sc::iracing namespace that returns a const reference to std::atomic<float>
    return tele_lap_percent;  // Return tele_lap_percent
}

const std::atomic<float> &sc::iracing::rpm() {  // Definition of a function "rpm" in the sc::iracing namespace that returns a const reference to std::atomic<float>
    return tele_rpm;  // Return tele_rpm
}

const std::atomic<float> &sc::iracing::rpm_prev() {  // Definition of a function "rpm_prev" in the sc::iracing namespace that returns a const reference to std::atomic<float>
    return tele_rpm_prev;  // Return tele_rpm_prev
}

const std::atomic<float> &sc::iracing::speed() {  // Definition of a function "speed" in the sc::iracing namespace that returns a const reference to std::atomic<float>
    return tele_speed;  // Return tele_speed
}

const std::atomic<float> &sc::iracing::speed_prev() {  // Definition of a function "speed_prev" in the sc::iracing namespace that returns a const reference to std::atomic<float>
    return tele_speed_prev;  // Return tele_speed_prev
}

const std::atomic<int> &sc::iracing::gear() {  // Definition of a function "gear" in the sc::iracing namespace that returns a const reference to std::atomic<int>
    return tele_gear;  // Return tele_gear
}

const std::atomic<int> &sc::iracing::gear_prev() {  // Definition of a function "gear_prev" in the sc::iracing namespace that returns a const reference to std::atomic<int>
    return tele_gear_prev;  // Return tele_gear_prev
}
