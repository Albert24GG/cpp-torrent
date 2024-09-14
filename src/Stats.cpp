#include "Stats.hpp"

#include <format>
#include <string>
#include <utility>

namespace torrent {

std::string Stats::get_formatted_download_rate() const {
    double download_rate{get_download_rate()};

    if (double megabyte_rate{download_rate / 1'048'576.0}; megabyte_rate >= 1.0) {
        return std::format("{:.2f} MiB/s", megabyte_rate);
    } else if (double kilobyte_rate{download_rate / 1'024.0}; kilobyte_rate >= 1.0) {
        return std::format("{:.2f} KiB/s", kilobyte_rate);
    } else {
        return std::format("{:.2f} B/s", download_rate);
    }

    std::unreachable();
}

std::string Stats::get_formatted_eta() const {
    auto eta{get_eta()};

    if (eta == std::chrono::seconds::max()) {
        return "Inf";
    }

    size_t eta_quant{static_cast<size_t>(eta.count())};
    size_t days_left{eta_quant / 86'400};
    size_t hours_left{(eta_quant % 86'400) / 3'600};
    size_t minutes_left{(eta_quant % 3'600) / 60};
    size_t seconds_left{eta_quant % 60};

    std::string formatted_eta{};

    if (days_left > 0) {
        formatted_eta += std::format("{}d:", days_left);
    }
    if (hours_left > 0) {
        formatted_eta += std::format("{}h:", hours_left);
    }
    if (minutes_left > 0) {
        formatted_eta += std::format("{}m:", minutes_left);
    }
    if (seconds_left >= 0) {
        formatted_eta += std::format("{}s", seconds_left);
    }

    return formatted_eta;
}

}  // namespace torrent
