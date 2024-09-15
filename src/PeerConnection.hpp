#pragma once

#include "Constant.hpp"
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

enum class PeerState { UNINITIATED, CONNECTING, CONNECTED, RUNNING, DISCONNECTED, TIMED_OUT };

class PeerConnection {
    public:
        PeerConnection(
            asio::io_context& io_context, PieceManager& piece_manager, PeerInfo peer_info
        )
            : peer_conn_ctx_{io_context},
              socket_{io_context},
              piece_manager_{piece_manager},
              peer_info_{std::move(peer_info)} {}

        /*
         * @brief Connect to the peer and perform the handshake.
         *
         * @param handshake_message the handshake message to send
         * @param info_hash         the info hash of the torrent
         */
        asio::awaitable<void> connect(
            const message::HandshakeMessage& handshake_message, const crypto::Sha1& info_hash
        );

        /**
         * @brief Start sending and receiving messages from the peer
         */
        asio::awaitable<void> run();

        /**
         * @brief Get the state of the peer connection
         *
         * @return The state of the peer connection
         */
        [[nodiscard]] PeerState get_state() const { return state_; }

        /**
         * @brief Get the number of retries left
         *
         * @return The number of retries left
         */
        [[nodiscard]] uint8_t get_retries_left() const { return retries_left_; }

        /**
         * @brief Disconnect the peer connection
         */
        void disconnect() {
            state_ = PeerState::DISCONNECTED;
            socket_.close();
        }

        /**
         * @brief Check if the peer was connected prior to the current state
         *
         * @return true if the peer was connected, false otherwise
         */
        [[nodiscard]] bool was_connected() const { return was_connected_; }

    private:
        /**
         * @brief Receive a handshake message from the peer
         *
         * @return The info hash of the peer if the handshake was successful, an error code
         * otherwise
         */
        auto receive_handshake() -> asio::awaitable<std::expected<crypto::Sha1, std::error_code>>;

        /**
         * @brief Send a handshake message to the peer
         *
         * @param handshake_message the handshake message to send
         * @return void if the handshake was successful, an error code otherwise
         */
        auto send_handshake(const message::HandshakeMessage& handshake_message
        ) -> asio::awaitable<std::expected<void, std::error_code>>;

        /**
         * @brief Establish a connection with the peer
         *
         * @return void if the connection was successful, an error code otherwise
         */
        auto establish_connection() -> asio::awaitable<std::expected<void, std::error_code>>;

        /**
         * @brief Load the next block requests in the send_buffer
         *
         * @param num_blocks the maximum number of blocks to request
         * @return the number of blocks requested
         */
        uint32_t load_block_requests(uint32_t num_blocks);

        /**
         * @brief Send the next block requests to the peer
         */
        asio::awaitable<void> send_requests();

        /**
         * @brief Receive messages from the peer
         */
        asio::awaitable<void> receive_messages();

        /**
         * @brief Handle a message received from the peer
         */
        asio::awaitable<void> handle_message(message::Message msg);

        /**
         * @brief Handle a failure in the connection
         *
         * @param ec the error code
         */
        asio::awaitable<void> handle_failure(std::error_code ec);

        /**
         * @brief Handle a have message
         *
         * @param payload the payload of the message
         */
        void handle_have_message(std::span<std::byte> payload);

        /**
         * @brief Handle a bitfield message
         *
         * @param payload the payload of the message
         */
        void handle_bitfield_message(std::span<std::byte> payload);

        /**
         * @brief Handle a piece message
         *
         * @param payload the payload of the message
         */
        void handle_piece_message(std::span<std::byte> payload);

        /**
         * @brief Reset the state of the peer connection
         * Used when connecting/reconnecting to a peer
         */
        void reset_state();

        asio::io_context&     peer_conn_ctx_;
        asio::ip::tcp::socket socket_;
        PieceManager&         piece_manager_;
        PeerInfo              peer_info_;
        uint8_t               retries_left_{MAX_RETRIES};

        // client is choking the peer
        bool am_choking_{true};
        // client is interested in the peer
        bool am_interested_{false};
        // peer is choking the client
        bool peer_choking_{true};
        // peer is interested in the client
        bool      peer_interested_{false};
        PeerState state_{PeerState::UNINITIATED};

        // Flag to indicate whether the peer was connected prior to the current state
        bool was_connected_{false};
        // Flag to indicate whether bitfield was received
        bool bitfield_received_{false};

        // Buffer for the sent messages
        std::vector<std::byte> send_buffer_;
        // Buffer for the received messages
        // Initialize the buffer with the size of the handshake message, and resize it after the
        // connection is done
        std::vector<std::byte> receive_buffer_{message::HANDSHAKE_MESSAGE_SIZE};

        std::vector<bool> bitfield_;

        uint32_t pending_block_requests_{0U};
};

};  // namespace torrent::peer
