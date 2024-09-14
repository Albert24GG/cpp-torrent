#pragma once

#ifdef DEBUG
#    define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include <filesystem>
#include <spdlog/spdlog.h>

#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(torrent::logger::get_instance(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(torrent::logger::get_instance(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(torrent::logger::get_instance(), __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(torrent::logger::get_instance(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(torrent::logger::get_instance(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(torrent::logger::get_instance(), __VA_ARGS__)

namespace torrent::logger {

enum class Level {
    trace    = SPDLOG_LEVEL_TRACE,
    debug    = SPDLOG_LEVEL_DEBUG,
    info     = SPDLOG_LEVEL_INFO,
    warn     = SPDLOG_LEVEL_WARN,
    error    = SPDLOG_LEVEL_ERROR,
    critical = SPDLOG_LEVEL_CRITICAL,
    off      = SPDLOG_LEVEL_OFF
};

/**
 * @brief Initialize the logger with the given log directory and log name
 *
 * @param log_dir The directory where the log file will be stored
 * @param log_name The name of the log file
 */
void init(const std::filesystem::path& log_file = "./log.txt");

/**
 * @brief Get the instance of the logger
 *
 * @return The instance of the logger
 */
spdlog::logger* get_instance();

/**
 * @brief Set the level of the logger
 *
 * @param level The level to set the logger to
 */
void set_level(Level level);

};  // namespace torrent::logger
