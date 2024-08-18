#pragma once

#include "PeerInfo.hpp"
#include "PieceManager.hpp"
#include "TorrentMessage.hpp"

#include <asio.hpp>
#include <chrono>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace torrent::peer {

enum class PeerState : uint8_t {
    UNINITIATED,
    CONNECTING,
    CONNECTED,
    RUNNING,
    DISCONNECTED,
    TIMED_OUT
};

class PeerConnection {
    public:
        PeerConnection(
            asio::io_context& io_context, PieceManager& piece_manager, PeerInfo peer_info
        )
            : peer_conn_ctx{io_context},
              socket{io_context},
              piece_manager{piece_manager},
              peer_info{std::move(peer_info)} {}

        /*
         * Connect to the peer and perform the handshake.
         *
         * @param handshake_message the handshake message to send
         * @param info_hash         the info hash of the torrent
         */
        asio::awaitable<void> connect(
            const message::HandshakeMessage& handshake_message, const crypto::Sha1& info_hash
        );

        [[nodiscard]] PeerState get_state() const { return state; }

    private:
        auto send_data(std::span<const std::byte> buffer
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        auto receive_data(std::span<std::byte> buffer
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        auto receive_handshake() -> asio::awaitable<std::expected<crypto::Sha1, std::error_code>>;

        auto send_handshake(const message::HandshakeMessage& handshake_message
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        auto establish_connection() -> asio::awaitable<std::expected<void, std::error_code>>;

        asio::io_context&     peer_conn_ctx;
        asio::ip::tcp::socket socket;
        PieceManager&         piece_manager;
        PeerInfo              peer_info;

        bool      am_choking{true};
        bool      am_interested{false};
        bool      peer_choking{true};
        bool      peer_interested{false};
        PeerState state{PeerState::UNINITIATED};
};

};  // namespace torrent::peer
