#include "PeerConnection.hpp"

#include "Crypto.hpp"
#include "Duration.hpp"
#include "Logger.hpp"
#include "TorrentMessage.hpp"
#include "Utils.hpp"

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <expected>
#include <ranges>
#include <span>
#include <variant>

using asio::awaitable;
using asio::ip::tcp;
using asio::use_awaitable;
namespace this_coro = asio::this_coro;
using namespace asio::experimental::awaitable_operators;

// Use the nothrow awaitable completion token to avoid exceptions
constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(use_awaitable);

namespace torrent::peer {

auto PeerConnection::receive_handshake()
    -> awaitable<std::expected<crypto::Sha1, std::error_code>> {
    LOG_DEBUG("Waiting for handshake message from peer {}:{}", peer_info_.ip, peer_info_.port);

    // Receive the handshake message
    auto handshake_result = co_await utils::tcp::receive_data_with_timeout(
        socket_,
        std::span<std::byte, message::HANDSHAKE_MESSAGE_SIZE>(receive_buffer_),
        duration::HANDSHAKE_TIMEOUT
    );

    if (!handshake_result.has_value()) {
        co_return std::unexpected(handshake_result.error());
    }

    // Parse the handshake message

    if (auto info_hash = message::parse_handshake_message(
            std::span<const std::byte, message::HANDSHAKE_MESSAGE_SIZE>(receive_buffer_)
        );
        !info_hash.has_value()) {
        co_return std::unexpected(std::error_code{});
    } else {
        co_return *info_hash;
    }
}

auto PeerConnection::send_handshake(const message::HandshakeMessage& handshake_message
) -> awaitable<std::expected<void, std::error_code>> {
    LOG_DEBUG("Sending handshake message to peer {}:{}", peer_info_.ip, peer_info_.port);

    auto res = co_await utils::tcp::send_data_with_timeout(
        socket_, handshake_message, duration::HANDSHAKE_TIMEOUT
    );

    co_return res;
}

auto PeerConnection::establish_connection() -> awaitable<std::expected<void, std::error_code>> {
    LOG_DEBUG("Establishing connection with peer {}:{}", peer_info_.ip, peer_info_.port);

    // Resolve the peer endpoint
    tcp::endpoint peer_endpoint = *tcp::resolver(co_await this_coro::executor)
                                       .resolve(peer_info_.ip, std::to_string(peer_info_.port));

    std::chrono::steady_clock::time_point deadline{
        std::chrono::steady_clock::now() + duration::CONNECTION_TIMEOUT
    };

    auto result = co_await (
        socket_.async_connect(peer_endpoint, use_nothrow_awaitable) || utils::watchdog(deadline)
    );

    std::expected<void, std::error_code> connection_result;

    std::visit(
        utils::visitor{
            [&connection_result](const std::expected<void, std::error_code>& res) {
                connection_result = res;
            },
            [&connection_result](const std::tuple<std::error_code>& res) {
                if (auto [ec] = res; ec) {
                    connection_result = std::unexpected(ec);
                }
            }
        },
        result
    );

    co_return connection_result;
}

uint32_t PeerConnection::load_block_requests(uint32_t num_blocks) {
    // Reset the buffer
    send_buffer_.clear();

    uint32_t blocks_requested{};

    for (auto i : std::views::iota(0U, num_blocks)) {
        auto request = piece_manager_.request_next_block(bitfield_);
        if (!request.has_value()) {
            break;
        }

        auto [piece_index, block_offset, block_size] = *request;
        send_buffer_.insert(send_buffer_.end(), 17, std::byte{0});

        message::create_request_message(
            std::span<std::byte>(send_buffer_).subspan(i * 17U, 17),
            piece_index,
            block_offset,
            block_size
        );

        ++blocks_requested;
    }

    return blocks_requested;
}

awaitable<void> PeerConnection::send_requests() {
    while (!piece_manager_.completed()) {
        co_await asio::steady_timer(co_await this_coro::executor, duration::REQUEST_INTERVAL)
            .async_wait(use_nothrow_awaitable);
        if (peer_choking_) {
            continue;
        }

        auto requested_blocks = load_block_requests(
            std::min(MAX_BLOCKS_IN_FLIGHT - pending_block_requests_, MAX_BLOCKS_PER_REQUEST)
        );

        if (requested_blocks == 0) {
            continue;
        }

        if (auto res = co_await utils::tcp::send_data_with_timeout(
                socket_, send_buffer_, duration::SEND_MSG_TIMEOUT
            );
            !res.has_value()) {
            LOG_DEBUG(
                "Failed to send request message to peer {}:{} with error:\n{}",
                peer_info_.ip,
                peer_info_.port,
                res.error().message()
            );
            co_await handle_failure(res.error());
            co_return;
        }

        pending_block_requests_ += requested_blocks;
    }
    co_return;
}

awaitable<void> PeerConnection::receive_messages() {
    while (!piece_manager_.completed()) {
        uint32_t message_size{};

        std::expected<void, std::error_code> res;
        res = co_await utils::tcp::receive_data_with_timeout(
            socket_,
            std::span<std::byte>(reinterpret_cast<std::byte*>(&message_size), 4),
            duration::RECEIVE_MSG_TIMEOUT
        );

        if (!res.has_value()) {
            LOG_DEBUG(
                "Failed to receive message size from peer {}:{} with error:\n{}",
                peer_info_.ip,
                peer_info_.port,
                res.error().message()
            );
            co_await handle_failure(res.error());
            co_return;
        }

        // Convert the message size to host byte order
        message_size = utils::network_to_host_order(message_size);

        // Keep-alive message
        if (message_size == 0) {
            continue;
        }

        std::byte id{};

        res = co_await utils::tcp::receive_data_with_timeout(
            socket_, std::span<std::byte>(&id, 1), duration::RECEIVE_MSG_TIMEOUT
        );

        if (!res.has_value()) {
            LOG_DEBUG(
                "Failed to receive message id from peer {}:{} with error:\n{}",
                peer_info_.ip,
                peer_info_.port,
                res.error().message()
            );
            co_await handle_failure(res.error());
            co_return;
        }

        std::optional<std::span<std::byte>> payload{std::nullopt};

        if (message_size > 1) {
            payload = std::span<std::byte>(receive_buffer_).subspan(0, message_size - 1);

            if (res = co_await utils::tcp::receive_data_with_timeout(
                    socket_, *payload, duration::RECEIVE_MSG_TIMEOUT
                );
                !res.has_value()) {
                LOG_DEBUG(
                    "Failed to receive message payload from peer {}:{} with error:\n{}",
                    peer_info_.ip,
                    peer_info_.port,
                    res.error().message()
                );
                co_await handle_failure(res.error());
                co_return;
            }
        }

        co_await handle_message({static_cast<message::MessageType>(id), payload});
    }
    co_return;
}

asio::awaitable<void> PeerConnection::handle_message(message::Message msg) {
    using message::MessageType;
    switch (msg.id) {
        case MessageType::CHOKE:
            peer_choking_ = true;
            break;
        case MessageType::UNCHOKE:
            peer_choking_ = false;
            break;
        case MessageType::INTERESTED:
            break;
        case MessageType::NOT_INTERESTED:
            break;
        case MessageType::HAVE:
            handle_have_message(*msg.payload);
            break;
        case MessageType::BITFIELD:
            handle_bitfield_message(*msg.payload);
            break;
        case MessageType::REQUEST:
            break;
        case MessageType::PIECE:
            handle_piece_message(*msg.payload);
            break;
    }
    co_return;
}

void PeerConnection::handle_have_message(std::span<std::byte> payload) {
    uint32_t piece_index{};
    std::ranges::copy(std::span<std::byte, 4>(payload), reinterpret_cast<std::byte*>(&piece_index));
    piece_index            = utils::network_to_host_order(piece_index);
    bitfield_[piece_index] = true;
    piece_manager_.add_available_piece(piece_index);
}

void PeerConnection::handle_bitfield_message(std::span<std::byte> payload) {
    for (auto i : std::views::iota(0U, bitfield_.size())) {
        bitfield_[i] = (static_cast<uint8_t>(payload[i >> 3]) >> (7U - (i & 7U))) & 1U;
    }
    piece_manager_.add_peer_bitfield(bitfield_);
    bitfield_received_ = true;
}

void PeerConnection::handle_piece_message(std::span<std::byte> payload) {
    auto parsed_message = message::parse_piece_message(payload);
    if (!parsed_message.has_value()) {
        return;
    }
    auto [piece_index, block_data, block_offset] = *parsed_message;
    piece_manager_.receive_block(piece_index, block_data, block_offset);
    --pending_block_requests_;
}

asio::awaitable<void> PeerConnection::handle_failure(std::error_code ec) {
    // Set the state appropriately
    state_ = ec == asio::error::timed_out ? PeerState::TIMED_OUT : PeerState::DISCONNECTED;
    // Close the socket
    socket_.close();

    co_return;
}

void PeerConnection::reset_state() {
    state_                  = PeerState::UNINITIATED;
    am_choking_             = true;
    am_interested_          = false;
    peer_choking_           = true;
    peer_interested_        = false;
    pending_block_requests_ = 0;
    bitfield_received_      = false;
    was_connected_          = false;
}

awaitable<void> PeerConnection::connect(
    const message::HandshakeMessage& handshake_message, const crypto::Sha1& info_hash
) {
    // Manage the retries
    if (retries_left_ == 0) {
        co_return;
    }
    --retries_left_;

    // Reset the state
    reset_state();

    // Set the state to connecting
    state_ = PeerState::CONNECTING;

    // Connect to the peer

    if (auto res = co_await establish_connection(); !res.has_value()) {
        LOG_DEBUG(
            "Failed to connect to peer {}:{} with error:\n{}",
            peer_info_.ip,
            peer_info_.port,
            res.error().message()
        );
        co_await handle_failure(res.error());
        co_return;
    }

    // Send the handshake message

    if (auto res = co_await send_handshake(handshake_message); !res.has_value()) {
        LOG_DEBUG(
            "Failed to send handshake message to peer {}:{} with error:\n{}",
            peer_info_.ip,
            peer_info_.port,
            res.error().message()
        );
        co_await handle_failure(res.error());
        co_return;
    }

    // Receive the handshake message

    if (auto res = co_await receive_handshake(); !res.has_value()) {
        LOG_DEBUG(
            "Failed to receive handshake message from peer {}:{} with error:\n{}",
            peer_info_.ip,
            peer_info_.port,
            res.error().message()
        );
        co_await handle_failure(res.error());
        co_return;
    } else if (*res != info_hash) {
        LOG_DEBUG(
            "Received invalid handshake message from peer {}:{}", peer_info_.ip, peer_info_.port
        );
        co_await handle_failure(asio::error::invalid_argument);
        co_return;
    }

    // Set the state to connected
    state_ = PeerState::CONNECTED;
    LOG_DEBUG("Successfully connected to peer {}:{}", peer_info_.ip, peer_info_.port);

    // Reset the retries
    retries_left_ = MAX_RETRIES;

    was_connected_ = true;

    co_return;

    // TODO: Also send bitfield message after implementing upload capabilities
}

awaitable<void> PeerConnection::run() {
    // Resize the send buffer to the max size of a message sent at once
    send_buffer_.resize(message::MAX_SENT_MSG_SIZE * MAX_BLOCKS_IN_FLIGHT);

    // Send interested message

    std::span<std::byte, 5> interested_message(send_buffer_);
    message::create_interested_message(interested_message);

    if (auto res = co_await utils::tcp::send_data_with_timeout(
            socket_, interested_message, duration::SEND_MSG_TIMEOUT
        );
        !res.has_value()) {
        LOG_DEBUG(
            "Failed to send interested message to peer {}:{} with error:\n{}",
            peer_info_.ip,
            peer_info_.port,
            res.error().message()
        );
        co_await handle_failure(res.error());
        co_return;
    }

    // Set the state to running

    state_ = PeerState::RUNNING;

    // Resize the bitfield

    bitfield_.resize(piece_manager_.get_piece_count());

    // Resize the receive buffer to the max size of a payload received at once
    // (either a piece msg or bitfield msg)

    receive_buffer_.resize(
        std::max(8 + static_cast<size_t>(BLOCK_SIZE), utils::ceil_div(bitfield_.size(), 8uz))
    );

    // Start the send requests and receive messages coroutines

    co_await (send_requests() || receive_messages());

    if (bitfield_received_) {
        piece_manager_.remove_peer_bitfield(bitfield_);
    }

    LOG_DEBUG("Peer {}:{} stopped running", peer_info_.ip, peer_info_.port);
}
}  // namespace torrent::peer
