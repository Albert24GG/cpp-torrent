#include "Logger.hpp"

#include "Error.hpp"

#include <memory>
#include <spdlog/sinks/basic_file_sink.h>

namespace {

std::shared_ptr<spdlog::logger> global_logger{nullptr};
torrent::logger::Level          global_level{torrent::logger::Level::info};

}  // namespace

namespace torrent::logger {

void init(const std::filesystem::path& log_dir, const std::string& log_name) {
    if (global_logger) {
        err::throw_with_trace("Logger already initialized");
    }

    if (global_level != Level::off) {
        global_logger = spdlog::basic_logger_mt("torrent-logger", log_dir / log_name);
        global_logger->set_level(static_cast<spdlog::level::level_enum>(global_level));
    }
}

spdlog::logger* get_instance() {
    // If the logger has not been initialized, initialize it with default values
    if (!global_logger) {
        init();
    }
    return global_logger.get();
}

void set_level(Level level) {
    global_level = level;
    if (global_logger) {
        global_logger->set_level(static_cast<spdlog::level::level_enum>(level));
    }
}

}  // namespace torrent::logger
