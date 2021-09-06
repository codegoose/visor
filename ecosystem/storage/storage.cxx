#include "storage.h"
#include "storage-object.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <map>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace sc::storage {

    static std::thread thread;
    static std::mutex mutex;
    static std::atomic_bool running = false;

    static std::map<std::string, bool> flag_queue;
    static std::vector<std::string> flag_delete_queue;
    static std::map<std::string, bool> flags;

    static std::map<std::string, YAML::Node> object_queue;
    static std::vector<std::string> object_delete_queue;
    static std::map<std::string, YAML::Node> objects;

    static void worker() {
        spdlog::debug("Storage worker thread has started.");
        for (;;) {
            if (!running) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (!mutex.try_lock()) continue;

            mutex.unlock();
        }
        spdlog::debug("Storage worker thread has ended.");
    }
}

std::optional<std::string> sc::storage::initialize() {
    if (const auto err = shutdown(); err) return err;
    running = true;
    thread = std::thread(worker);
    return std::nullopt;
}

std::optional<std::string> sc::storage::shutdown() {
    if (!running) return std::nullopt;
    spdlog::debug("Waiting for storage worker to finish...");
    running = false;
    thread.join();
    return std::nullopt;
}

std::optional<std::string> sc::storage::sync() {
    mutex.lock();
    {
        for (auto &key : flag_delete_queue) flags.erase(key);
        flag_delete_queue.clear();
        for (auto &pair : flag_queue) flags[pair.first] = pair.second;
        flag_queue.clear();
        YAML::Emitter out;
        out.SetOutputCharset(YAML::EscapeNonAscii);
        out.SetIndent(2);
        out << YAML::BeginMap;
        for (auto &pair : flags) {
            out << YAML::Key << pair.first << YAML::Value << pair.second;
        }
        out << YAML::EndMap;
    }
    {
        for (auto &key : object_delete_queue) objects.erase(key);
        object_delete_queue.clear();
        for (auto &pair : object_queue) objects[pair.first] = pair.second;
        object_queue.clear();
        YAML::Emitter out;
        out.SetOutputCharset(YAML::EscapeNonAscii);
        out.SetIndent(2);
        out << YAML::BeginMap;
        for (auto &pair : objects) {
            out << YAML::Key << pair.first << YAML::Value << pair.second;
        }
        out << YAML::EndMap;
        fmt::print("{}\n", out.c_str());
    }
    mutex.unlock();
    return std::nullopt;
}

void sc::storage::set_flag(const std::string_view &key, const bool &value) {
    std::lock_guard guard(mutex);
    flag_queue[key.data()] = value;
    spdlog::debug("Set storage flag: {} -> {}", key, value);
}

tl::expected<bool, std::string> sc::storage::get_flag(const std::string_view &key, const std::optional<bool> &default_value) {
    const auto i = flags.find(key.data());
    if (i == flags.end()) {
        if (default_value) return *default_value;
        else {
            spdlog::warn("Storage flag not present: {}", key);
            return tl::make_unexpected("Key does not exist.");
        }
    }
    return i->second;
}

void sc::storage::set_object(const std::string_view name, const YAML::Emitter &object) {
    std::lock_guard guard(mutex);
    object_queue[name.data()] = object.c_str();
    spdlog::debug("Set storage object: {}", name);
}

tl::expected<YAML::Node, std::string> sc::storage::get_object(const std::string_view &key) {
    const auto i = objects.find(key.data());
    if (i == objects.end()) return tl::make_unexpected("Key does not exist");
    return i->second;
}