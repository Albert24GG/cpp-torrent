#pragma once

#include "Crypto.hpp"
#include "Error.hpp"
#include "PeerConnection.hpp"
#include "PeerInfo.hpp"
#include "PieceManager.hpp"
#include "TorrentMessage.hpp"

#include <array>
#include <asio.hpp>
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

namespace torrent {

class PeerManager {
    public:
        PeerManager(
            std::shared_ptr<PieceManager> piece_manager,
            const crypto::Sha1&           info_hash,
            const std::string&            peer_id
        )
            : piece_manager_(std::move(piece_manager)), info_hash_(info_hash) {
            if (peer_id.size() != 20) {
                err::throw_with_trace("Peer ID must be 20 bytes long");
            }
            handshake_message_ = message::create_handshake_message(
                info_hash, std::span<const char, 20>(peer_id.data(), 20)
            );
        }

        PeerManager(const PeerManager&)            = delete;
        PeerManager& operator=(const PeerManager&) = delete;
        PeerManager(PeerManager&&)                 = delete;
        PeerManager& operator=(PeerManager&&)      = delete;

        ~PeerManager() { stop(); }

        /**
         * @brief Start the peer manager
         */
        void start();

        /**
         * @brief Stop the peer manager
         */
        void stop();

        /**
         * @brief Add peers to the peer manager
         *
         * @param peers The peers to add
         * @note Peers that cannot be connected will be dropped.
         */
        void add_peers(std::span<PeerInfo> peers);

        /**
         * @brief Get the number of active connections
         *
         * @return The number of active connections
         * @note This function is thread-safe
         */
        uint32_t get_connected_peers() const {
            return connected_peers_.load(std::memory_order_relaxed);
        }

    private:
        /**
         * @brief Try to reconnect to a peer
         *
         * @param peer_info The peer info
         * @param peer The peer connection
         * @param is_reconnecting A reference to a boolean that will be set to true if the peer is
         *                        currently being reconnected
         */
        asio::awaitable<void> try_reconnection(
            const PeerInfo& peer_info, peer::PeerConnection& peer, bool& is_reconnecting
        );

        /**
         * @brief Cleanup the peer connections
         * This function will remove all the peer connections that have the state set to
         * DISCONNECTED and will try to reconnect to the peers that have the state set to TIMED_OUT
         *
         * @note This function will run as long as the peer manager is running
         */
        asio::awaitable<void> cleanup_peer_connections();

        asio::io_context                                           peer_conn_ctx_;
        asio::executor_work_guard<asio::io_context::executor_type> peer_conn_work_guard_{
            asio::make_work_guard(peer_conn_ctx_)
        };

        asio::io_context                                           utils_ctx_;
        asio::executor_work_guard<asio::io_context::executor_type> utils_work_guard_{
            asio::make_work_guard(utils_ctx_)
        };

        // Mutex to protect the peer_connections map from concurrent access (adding peers + cleanup)
        std::mutex peer_connections_mutex_;

        // Run the peer connection context in a separate thread
        std::jthread peer_connection_thread_;
        // Run the utility context in a separate thread
        std::jthread utils_thread_;

        // Map of peer connections
        // the bool value indicates whether the peer is currently being reconnected
        std::unordered_map<PeerInfo, std::pair<peer::PeerConnection, bool>> peer_connections_;
        std::shared_ptr<PieceManager>                                       piece_manager_;
        message::HandshakeMessage                                           handshake_message_;
        crypto::Sha1                                                        info_hash_;
        bool                                                                started_{false};
        std::atomic<uint32_t>                                               connected_peers_{0};
};

};  // namespace torrent
