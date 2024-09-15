#pragma once

#include "Constant.hpp"
#include "Duration.hpp"
#include "TorrentClient.hpp"

#include <atomic>
#include <indicators/block_progress_bar.hpp>
#include <thread>

namespace torrent::ui {

class ProgressBar {
    public:
        explicit ProgressBar(TorrentClient& torrent_client)
            : bar_(
                  indicators::option::BarWidth{PROGRESS_BAR_WIDTH},
                  indicators::option::PostfixText{PROGRESS_BAR_INIT_TEXT}
              ),
              torrent_client_{torrent_client} {}

        /**
         * Starts drawing the progress bar.
         *
         * @Note: This function will block until the torrent client has started. Should be called
         * from a separate thread.
         */
        void start_draw();

        /**
         * Set the stop flag to true.
         * This will stop the progress bar from drawing.
         */
        void stop_draw() { stop_flag_.test_and_set(std::memory_order_relaxed); }

    private:
        indicators::BlockProgressBar bar_;
        TorrentClient&               torrent_client_;
        std::atomic_flag             stop_flag_{ATOMIC_FLAG_INIT};
        std::jthread                 draw_thread_;
        std::chrono::milliseconds    refresh_rate_{duration::PROGRESS_BAR_REFRESH_RATE};
};
}  // namespace torrent::ui
