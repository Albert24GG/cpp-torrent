#pragma once

#include "Crypto.hpp"
#include "PeerConnection.hpp"
#include "PeerInfo.hpp"
#include "PieceManager.hpp"
#include "TorrentMessage.hpp"

#include <array>
#include <asio.hpp>
#include <cstddef>
#include <memory>
#include <thread>
#include <unordered_map>

namespace torrent {

constexpr auto CONN_CLEANUP_INTERVAL = std::chrono::seconds{10};

class PeerManager {
    public:
        PeerManager(
            std::shared_ptr<PieceManager> piece_manager,
            const crypto::Sha1&           info_hash,
            std::span<const char, 20>     peer_id
        )
            : piece_manager{std::move(piece_manager)},
              info_hash{info_hash},
              handshake_message{message::create_handshake_message(info_hash, peer_id)} {}

        PeerManager(const PeerManager&)            = delete;
        PeerManager& operator=(const PeerManager&) = delete;
        PeerManager(PeerManager&&)                 = delete;
        PeerManager& operator=(PeerManager&&)      = delete;

        ~PeerManager() { stop(); }

        void start();

        void stop();

        void add_peers(std::span<PeerInfo> peers);

    private:
        asio::awaitable<void> handle_download_completion();

        asio::awaitable<void> cleanup_peer_connections();

        asio::io_context                                           peer_conn_ctx;
        asio::executor_work_guard<asio::io_context::executor_type> peer_conn_work_guard{
            asio::make_work_guard(peer_conn_ctx)
        };

        // Mutex to protect the peer_connections map from concurrent access (adding peers + cleanup)
        std::mutex peer_connections_mutex;

        // Use a thread pool to initiate the peer connections
        asio::thread_pool conn_init_thread_pool{1};

        // Run the peer connection context in a separate thread
        std::jthread peer_connection_thread;

        std::unordered_map<PeerInfo, peer::PeerConnection> peer_connections;
        std::shared_ptr<PieceManager>                      piece_manager;
        message::HandshakeMessage                          handshake_message;
        crypto::Sha1                                       info_hash;
        bool                                               started{false};
};

};  // namespace torrent
