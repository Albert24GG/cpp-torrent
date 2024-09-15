#include "TorrentClient.hpp"

#include "Constant.hpp"
#include "Error.hpp"
#include "FileManager.hpp"
#include "Logger.hpp"
#include "PeerRetriever.hpp"
#include "Utils.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

namespace torrent {

TorrentClient::TorrentClient(
    std::filesystem::path torrent_file, std::filesystem::path output_dir, uint16_t port
) {
    std::ifstream torrent_istream(torrent_file, std::ios::binary | std::ios::in);

    if (!torrent_istream.is_open()) {
        err::throw_with_trace("Failed to open torrent file");
    }

    torrent_md_ = md::parse_torrent_file(torrent_istream);

    file_manager_ = std::make_shared<fs::FileManager>(torrent_md_.files, output_dir);

    piece_manager_ = std::make_shared<PieceManager>(
        torrent_md_.piece_length,
        file_manager_->get_total_length(),
        file_manager_,
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(torrent_md_.piece_hashes.data()),
            torrent_md_.piece_hashes.size()
        )
    );

    // Generate the client ID
    std::string client_id{
        std::format("{}{}", CLIENT_ID_BASE, utils::generate_random<uint64_t>(1e11, 1e12 - 1))
    };

    peer_manager_ = std::make_shared<PeerManager>(piece_manager_, torrent_md_.info_hash, client_id);

    peer_retriever_ = std::make_shared<PeerRetriever>(
        torrent_md_.announce,
        torrent_md_.announce_list,
        torrent_md_.info_hash,
        client_id,
        port,
        file_manager_->get_total_length()
    );

    // set the total_bytes field in stats
    stats_.total_bytes = file_manager_->get_total_length();
}

void TorrentClient::update_stats() const {
    stats_.downloaded_bytes = piece_manager_->get_downloaded_bytes();
    stats_.connected_peers  = peer_manager_->get_connected_peers();
}

void TorrentClient::start_download() {
    auto peers = peer_retriever_->retrieve_peers(0);

    if (!peers.has_value()) {
        err::throw_with_trace("Failed to retrieve peers from the tracker");
    }

    peer_manager_->start();

    // Mark the start of the download
    stats_.start_time = std::chrono::steady_clock::now();
    download_status_.store(DownloadStatus::DOWNLOADING, std::memory_order_release);

    peer_manager_->add_peers(*peers);

    auto next_request_time{std::chrono::steady_clock::now() + peer_retriever_->get_interval()};

    while (!piece_manager_->completed_thread_safe()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (std::chrono::steady_clock::now() >= next_request_time) {
            peers = peer_retriever_->retrieve_peers(piece_manager_->get_downloaded_bytes());
            if (peers.has_value()) {
                peer_manager_->add_peers(*peers);
            }
            next_request_time = std::chrono::steady_clock::now() + peer_retriever_->get_interval();
        }
    }

    // Mark the end of the download
    LOG_INFO("Download completed");
    peer_manager_->stop();
    download_status_.store(DownloadStatus::FINISHED, std::memory_order_release);
}

}  // namespace torrent
