#pragma once

#include <format>
#include <source_location>
#include <string>

/**
 * @brief Get error message with source location
 * @param msg Error message
 * @param loc Source location
 * @return Error message with source location
 */

[[nodiscard]] inline std::string get_err_msg(
    std::string&& msg, const std::source_location loc = std::source_location::current()) {
    return std::string{std::format("file: {}({}:{}) {} -- {}",
                                   loc.file_name(),
                                   loc.line(),
                                   loc.column(),
                                   loc.function_name(),
                                   msg)};
}
