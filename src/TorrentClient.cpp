#include "TorrentClient.hpp"

#include "Error.hpp"
#include "FileManager.hpp"
#include "PeerRetriever.hpp"
#include "Utils.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

namespace torrent {

static constexpr std::string CLIENT_ID_BASE{"-qB4650-"};

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
        CLIENT_ID_BASE + std::to_string(utils::generate_random<uint64_t>(
                             static_cast<uint64_t>(1e11), static_cast<uint64_t>(1e12) - 1
                         ))
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
}

void TorrentClient::start_download() {
    auto peers = peer_retriever_->retrieve_peers(0);

    if (!peers.has_value()) {
        err::throw_with_trace("Failed to retrieve peers from the tracker");
    }

    peer_manager_->add_peers(*peers);

    auto next_request_time{std::chrono::steady_clock::now() + peer_retriever_->get_interval()};

    while (!piece_manager_->completed()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (std::chrono::steady_clock::now() >= next_request_time) {
            peers = peer_retriever_->retrieve_peers(piece_manager_->get_downloaded_bytes());
            if (peers.has_value()) {
                peer_manager_->add_peers(*peers);
            }
            next_request_time = std::chrono::steady_clock::now() + peer_retriever_->get_interval();
        }
    }
}

}  // namespace torrent
