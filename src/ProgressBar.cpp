#include "ProgressBar.hpp"

#include "Stats.hpp"
#include "TorrentClient.hpp"

#include <format>
#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <thread>

namespace torrent::ui {

void ProgressBar::start_draw() {
    indicators::show_console_cursor(false);

    // We need to wait until the torrent client has started downloading
    while (torrent_client_.get_download_status() != DownloadStatus::DOWNLOADING &&
           !stop_flag_.test(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    while (torrent_client_.get_download_status() != DownloadStatus::FINISHED &&
           !stop_flag_.test(std::memory_order_relaxed)) {
        const Stats cur_stats = torrent_client_.get_stats();

        std::string bar_postfix{std::format(
            " {} | ETA: {} | Peers: {}",
            cur_stats.get_formatted_download_rate(),
            cur_stats.get_formatted_eta(),
            cur_stats.connected_peers
        )};
        bar_.set_option(indicators::option::PostfixText{bar_postfix});
        bar_.set_progress(100.0 * cur_stats.get_download_percentage());

        std::this_thread::sleep_for(std::chrono::milliseconds(1'000));
    }
    indicators::show_console_cursor(true);
}

}  // namespace torrent::ui
