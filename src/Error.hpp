#pragma once

#include <stdexcept>
#include <string>

namespace err {

/**
 * @brief Get the error message with trace
 *
 * @param msg The error message
 * @return std::string The error message with trace
 */
[[nodiscard]] std::string err_msg_with_trace(const std::string& msg);

/**
 * @brief Throw an exception with trace
 *
 * @tparam Error The type of the exception
 * @param msg The error message
 */
template <typename Error = std::runtime_error>
[[noreturn]] void throw_with_trace(const std::string& msg) {
    throw Error(err_msg_with_trace(msg));
}
}  // namespace err
