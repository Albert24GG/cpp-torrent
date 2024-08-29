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

// Max blocks that can be requested from a peer at a time
inline constexpr auto MAX_BLOCKS_IN_FLIGHT = 5;

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

        asio::awaitable<void> run();

        [[nodiscard]] PeerState get_state() const { return state; }

    private:
        auto send_data(std::span<const std::byte> buffer
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        auto send_data_with_timeout(
            std::span<const std::byte> buffer, std::chrono::milliseconds timeout
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        auto receive_data(std::span<std::byte> buffer
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        auto receive_data_with_timeout(
            std::span<std::byte> buffer, std::chrono::milliseconds timeout
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        auto receive_handshake() -> asio::awaitable<std::expected<crypto::Sha1, std::error_code>>;

        auto send_handshake(const message::HandshakeMessage& handshake_message
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        auto establish_connection() -> asio::awaitable<std::expected<void, std::error_code>>;

        void                  load_block_requests();
        asio::awaitable<void> send_requests();
        asio::awaitable<void> receive_messages();
        asio::awaitable<void> handle_message(message::Message msg);

        void handle_have_message(std::span<std::byte> payload);
        void handle_bitfield_message(std::span<std::byte> payload);
        void handle_piece_message(std::span<std::byte> payload);

        asio::io_context&     peer_conn_ctx;
        asio::ip::tcp::socket socket;
        PieceManager&         piece_manager;
        PeerInfo              peer_info;
        uint8_t               retries_left{};

        // client is choking the peer
        bool am_choking{true};
        // client is interested in the peer
        bool am_interested{false};
        // peer is choking the client
        bool peer_choking{true};
        // peer is interested in the client
        bool      peer_interested{false};
        PeerState state{PeerState::UNINITIATED};

        // Buffer for the sent messages
        std::vector<std::byte> send_buffer;
        // Buffer for the received messages
        // Initialize the buffer with the size of the handshake message, and resize it after the
        // connection is done
        std::vector<std::byte> receive_buffer{message::HANDSHAKE_MESSAGE_SIZE};

        std::vector<bool> bitfield;
};

};  // namespace torrent::peer
