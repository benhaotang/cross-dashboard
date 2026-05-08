#pragma once

#include "domain/models.h"

#include <chrono>
#include <string>

namespace cd {

/** Port of Kotlin `TaskInputParser`. */
struct TaskInputParser {
    [[nodiscard]] static ParsedTask parse(
        std::string const& input,
        TaskDefaults const& defaults = {},
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now());
};

} // namespace cd
