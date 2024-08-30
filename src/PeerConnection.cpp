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

namespace {

// A visitor to use with std::visit on a variant
template <typename... Callable>
struct visitor : Callable... {
        using Callable::operator()...;
};

awaitable<std::expected<void, std::error_code>> watchdog(
    asio::chrono::steady_clock::time_point& deadline
) {
    asio::steady_timer timer{co_await this_coro::executor};

    auto now = std::chrono::steady_clock::now();

    while (deadline > now) {
        timer.expires_at(deadline);
        co_await timer.async_wait(use_nothrow_awaitable);
        now = std::chrono::steady_clock::now();
    }

    LOG_DEBUG("Watchdog timer expired");
    co_return std::unexpected(asio::error::timed_out);
}

}  // namespace

namespace torrent::peer {

auto PeerConnection::send_data(std::span<const std::byte> buffer
) -> awaitable<std::expected<void, std::error_code>> {
    // Send the data
    auto [e, n] = co_await asio::async_write(socket, asio::buffer(buffer), use_nothrow_awaitable);

    if (e) {
        LOG_DEBUG(
            "Failed to send data to {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            e.message()
        );
        co_return std::unexpected(e);
    }

    co_return std::expected<void, std::error_code>{};
}

auto PeerConnection::send_data_with_timeout(
    std::span<const std::byte> buffer, std::chrono::milliseconds timeout
) -> asio::awaitable<std::expected<void, std::error_code>> {
    std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + timeout};

    auto result = co_await (send_data(buffer) || watchdog(deadline));
    std::expected<void, std::error_code> return_res;

    std::visit(
        visitor{[&return_res](const std::expected<void, std::error_code>& res) { return_res = res; }
        },
        result
    );

    co_return return_res;
}

auto PeerConnection::receive_data(std::span<std::byte> buffer
) -> awaitable<std::expected<void, std::error_code>> {
    // Receive the data
    auto [e, n] = co_await asio::async_read(socket, asio::buffer(buffer), use_nothrow_awaitable);

    if (e) {
        LOG_DEBUG(
            "Failed to receive data from {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            e.message()
        );
        co_return std::unexpected(e);
    }

    co_return std::expected<void, std::error_code>{};
}

auto PeerConnection::receive_data_with_timeout(
    std::span<std::byte> buffer, std::chrono::milliseconds timeout
) -> asio::awaitable<std::expected<void, std::error_code>> {
    std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + timeout};

    auto result = co_await (receive_data(buffer) || watchdog(deadline));
    std::expected<void, std::error_code> return_res;

    std::visit(
        visitor{[&return_res](const std::expected<void, std::error_code>& res) { return_res = res; }
        },
        result
    );

    co_return return_res;
}

auto PeerConnection::receive_handshake()
    -> awaitable<std::expected<crypto::Sha1, std::error_code>> {
    LOG_DEBUG("Waiting for handshake message from peer {}:{}", peer_info.ip, peer_info.port);

    // Receive the handshake message
    auto handshake_result = co_await receive_data_with_timeout(
        std::span<std::byte, message::HANDSHAKE_MESSAGE_SIZE>(receive_buffer),
        duration::HANDSHAKE_TIMEOUT
    );

    if (!handshake_result.has_value()) {
        co_return std::unexpected(handshake_result.error());
    }

    // Parse the handshake message

    if (auto info_hash = message::parse_handshake_message(
            std::span<const std::byte, message::HANDSHAKE_MESSAGE_SIZE>(receive_buffer)
        );
        !info_hash.has_value()) {
        co_return std::unexpected(std::error_code{});
    } else {
        co_return *info_hash;
    }
}

auto PeerConnection::send_handshake(const message::HandshakeMessage& handshake_message
) -> awaitable<std::expected<void, std::error_code>> {
    LOG_DEBUG("Sending handshake message to peer {}:{}", peer_info.ip, peer_info.port);

    auto res = co_await send_data_with_timeout(handshake_message, duration::HANDSHAKE_TIMEOUT);

    co_return res;
}

auto PeerConnection::establish_connection() -> awaitable<std::expected<void, std::error_code>> {
    LOG_DEBUG("Establishing connection with peer {}:{}", peer_info.ip, peer_info.port);

    // Resolve the peer endpoint
    tcp::endpoint peer_endpoint = *tcp::resolver(co_await this_coro::executor)
                                       .resolve(peer_info.ip, std::to_string(peer_info.port));

    std::chrono::steady_clock::time_point deadline{
        std::chrono::steady_clock::now() + duration::CONNECTION_TIMEOUT
    };

    auto result =
        co_await (socket.async_connect(peer_endpoint, use_nothrow_awaitable) || watchdog(deadline));

    std::expected<void, std::error_code> connection_result;

    std::visit(
        visitor{
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

void PeerConnection::load_block_requests() {
    // Reset the buffer
    send_buffer.clear();

    for (auto i : std::views::iota(0, MAX_BLOCKS_IN_FLIGHT)) {
        auto request = piece_manager.request_next_block(bitfield);
        if (!request.has_value()) {
            break;
        }

        auto [piece_index, block_offset, block_size] = *request;
        send_buffer.insert(send_buffer.end(), 17, std::byte{0});

        message::create_request_message(
            std::span<std::byte>(send_buffer).subspan(i * 17, 17),
            piece_index,
            block_offset,
            block_size
        );
    }
}

awaitable<void> PeerConnection::send_requests() {
    while (!piece_manager.completed()) {
        co_await asio::steady_timer(co_await this_coro::executor, duration::REQUEST_INTERVAL)
            .async_wait(use_nothrow_awaitable);
        if (peer_choking) {
            continue;
        }
        load_block_requests();

        if (send_buffer.empty()) {
            continue;
        }

        if (auto res = co_await send_data_with_timeout(send_buffer, duration::SEND_MSG_TIMEOUT);
            !res.has_value()) {
            LOG_DEBUG(
                "Failed to send request message to peer {}:{} with error:\n{}",
                peer_info.ip,
                peer_info.port,
                res.error().message()
            );
            co_await handle_failure(res.error());
            co_return;
        }
    }
    co_return;
}

awaitable<void> PeerConnection::receive_messages() {
    while (!piece_manager.completed()) {
        uint32_t message_size{};

        std::expected<void, std::error_code> res;
        res = co_await receive_data_with_timeout(
            std::span<std::byte>(reinterpret_cast<std::byte*>(&message_size), 4),
            duration::RECEIVE_MSG_TIMEOUT
        );

        if (!res.has_value()) {
            LOG_DEBUG(
                "Failed to receive message size from peer {}:{} with error:\n{}",
                peer_info.ip,
                peer_info.port,
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

        res = co_await receive_data_with_timeout(
            std::span<std::byte>(&id, 1), duration::RECEIVE_MSG_TIMEOUT
        );

        if (!res.has_value()) {
            LOG_DEBUG(
                "Failed to receive message id from peer {}:{} with error:\n{}",
                peer_info.ip,
                peer_info.port,
                res.error().message()
            );
            co_await handle_failure(res.error());
            co_return;
        }

        std::optional<std::span<std::byte>> payload{std::nullopt};

        if (message_size > 1) {
            payload = std::span<std::byte>(receive_buffer).subspan(0, message_size - 1);

            if (res = co_await receive_data_with_timeout(*payload, duration::RECEIVE_MSG_TIMEOUT);
                !res.has_value()) {
                LOG_DEBUG(
                    "Failed to receive message payload from peer {}:{} with error:\n{}",
                    peer_info.ip,
                    peer_info.port,
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
            peer_choking = true;
            break;
        case MessageType::UNCHOKE:
            peer_choking = false;
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
    piece_index           = utils::network_to_host_order(piece_index);
    bitfield[piece_index] = true;
    piece_manager.add_available_piece(piece_index);
}

void PeerConnection::handle_bitfield_message(std::span<std::byte> payload) {
    for (auto i : std::views::iota(0U, bitfield.size())) {
        bitfield[i] = (static_cast<uint8_t>(payload[i >> 3]) >> (7U - (i & 7U))) & 1U;
    }
    piece_manager.add_peer_bitfield(bitfield);
}

void PeerConnection::handle_piece_message(std::span<std::byte> payload) {
    auto parsed_message = message::parse_piece_message(payload);
    if (!parsed_message.has_value()) {
        return;
    }
    auto [piece_index, block_data, block_offset] = *parsed_message;
    piece_manager.receive_block(piece_index, block_data, block_offset);
}

asio::awaitable<void> PeerConnection::handle_failure(std::error_code ec) {
    // Set the state appropriately
    state = ec == asio::error::timed_out ? PeerState::TIMED_OUT : PeerState::DISCONNECTED;
    // Close the socket
    socket.close();
    co_return;
}

awaitable<void> PeerConnection::connect(
    const message::HandshakeMessage& handshake_message, const crypto::Sha1& info_hash
) {
    // Manage the retries
    if (retries_left == 0) {
        co_return;
    }
    --retries_left;

    // Set the state to connecting
    state = PeerState::CONNECTING;

    // Connect to the peer

    if (auto res = co_await establish_connection(); !res.has_value()) {
        LOG_DEBUG(
            "Failed to connect to peer {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            res.error().message()
        );
        co_await handle_failure(res.error());
        co_return;
    }

    // Send the handshake message

    if (auto res = co_await send_handshake(handshake_message); !res.has_value()) {
        LOG_DEBUG(
            "Failed to send handshake message to peer {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            res.error().message()
        );
        co_await handle_failure(res.error());
        co_return;
    }

    // Receive the handshake message

    if (auto res = co_await receive_handshake(); !res.has_value()) {
        LOG_DEBUG(
            "Failed to receive handshake message from peer {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            res.error().message()
        );
        co_await handle_failure(res.error());
        co_return;
    } else if (*res != info_hash) {
        LOG_DEBUG(
            "Received invalid handshake message from peer {}:{}", peer_info.ip, peer_info.port
        );
        co_await handle_failure(asio::error::invalid_argument);
        co_return;
    }

    // Set the state to connected
    state = PeerState::CONNECTED;
    LOG_DEBUG("Successfully connected to peer {}:{}", peer_info.ip, peer_info.port);

    co_return;

    // TODO: Also send bitfield message after implementing upload capabilities
}

awaitable<void> PeerConnection::run() {
    // Resize the send buffer to the max size of a message sent at once
    send_buffer.resize(message::MAX_SENT_MSG_SIZE * MAX_BLOCKS_IN_FLIGHT);

    // Send interested message

    std::span<std::byte, 5> interested_message(send_buffer);
    message::create_interested_message(interested_message);

    if (auto res = co_await send_data_with_timeout(interested_message, duration::SEND_MSG_TIMEOUT);
        !res.has_value()) {
        LOG_DEBUG(
            "Failed to send interested message to peer {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            res.error().message()
        );
        co_await handle_failure(res.error());
        co_return;
    }

    // Set the state to running

    state = PeerState::RUNNING;

    // Resize the bitfield

    bitfield.resize(piece_manager.get_piece_count());

    // Resize the receive buffer to the max size of a payload received at once
    // (either a piece msg or bitfield msg)

    receive_buffer.resize(
        std::max(8 + static_cast<size_t>(BLOCK_SIZE), utils::ceil_div(bitfield.size(), 8uz))
    );

    // Start the send requests and receive messages coroutines

    co_await (send_requests() || receive_messages());
    piece_manager.remove_peer_bitfield(bitfield);
    LOG_DEBUG("Peer {}:{} stopped running", peer_info.ip, peer_info.port);
}
}  // namespace torrent::peer
