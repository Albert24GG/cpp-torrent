#pragma once

#include "FileManager.hpp"
#include "PeerManager.hpp"
#include "PeerRetriever.hpp"
#include "PieceManager.hpp"
#include "Stats.hpp"
#include "TorrentMetadata.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace torrent {

enum class DownloadStatus { STOPPED, DOWNLOADING, FINISHED };

class TorrentClient {
    public:
        TorrentClient(
            std::filesystem::path torrent_file,
            std::filesystem::path output_dir = ".",
            uint16_t              port       = 6'881
        );

        /**
         * Start the download process.
         */
        void start_download();

        /**
         * Get the stats of the torrent client.
         *
         * @return The stats struct of the torrent client.
         */
        [[nodiscard]] const Stats& get_stats() const {
            update_stats();
            return stats_;
        }

        /**
         * Get the download status of the torrent client.
         *
         * @return The download status of the torrent client.
         * @Note This function is thread-safe.
         */
        [[nodiscard]]
        DownloadStatus get_download_status() const {
            return download_status_.load(std::memory_order_acquire);
        }

    private:
        void update_stats() const;

        md::TorrentMetadata              torrent_md_;
        std::shared_ptr<fs::FileManager> file_manager_;
        std::shared_ptr<PieceManager>    piece_manager_;
        std::shared_ptr<PeerManager>     peer_manager_;
        std::shared_ptr<PeerRetriever>   peer_retriever_;
        mutable Stats                    stats_;
        std::atomic<DownloadStatus>      download_status_{DownloadStatus::STOPPED};
};

}  // namespace torrent
