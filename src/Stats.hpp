#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace torrent {

struct Stats {
        /**
         * @brief Get the elapsed seconds since the start of the download
         *
         * @return The elapsed seconds
         */
        [[nodiscard]] auto get_elapsed_seconds() const -> std::chrono::seconds {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time
            );
        }

        /*
         * @brief Get the download rate in bytes per second
         *
         * @return The download rate
         */
        [[nodiscard]] double get_download_rate() const {
            return static_cast<double>(downloaded_bytes) / get_elapsed_seconds().count();
        }

        /**
         * @brief Get the download rate in a formatted string
         *
         * @return The download rate as a string
         * @note The rate is formatted as B/s, KiB/s or MiB/s
         * depending on the magnitude of the rate
         */
        [[nodiscard]] std::string get_formatted_download_rate() const;

        /**
         * @brief Get the download percentage
         *
         * @return The download percentage
         */
        [[nodiscard]] double get_download_percentage() const {
            return static_cast<double>(downloaded_bytes) / total_bytes;
        }

        /**
         * @brief Get the estimated time of arrival in seconds
         *
         * @return The estimated time of arrival
         */
        [[nodiscard]] auto get_eta() const -> std::chrono::seconds {
            return std::chrono::seconds{
                static_cast<long>((total_bytes - downloaded_bytes) / get_download_rate())
            };
        }

        /**
         * @brief Get the estimated time of arrival in a formatted string
         *
         * @return The estimated time of arrival as a string
         * @note The time is formatted as DD:HH:MM:SS
         */
        [[nodiscard]] std::string get_formatted_eta() const;

        size_t                                             total_bytes{};
        size_t                                             downloaded_bytes{};
        std::chrono::time_point<std::chrono::steady_clock> start_time;
        uint16_t                                           connected_peers{};
};

}  // namespace torrent
