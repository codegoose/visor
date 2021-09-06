#pragma once

#include "storage.h"

#include <yaml-cpp/yaml.h>

namespace sc::storage {

    void set_object(const std::string_view name, const YAML::Emitter &object);
    tl::expected<YAML::Node, std::string> get_object(const std::string_view &key);
}