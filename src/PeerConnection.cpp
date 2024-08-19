#include "PeerConnection.hpp"

#include "asio/use_awaitable.hpp"
#include "Crypto.hpp"
#include "Duration.hpp"
#include "TorrentMessage.hpp"

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/cancellation_condition.hpp>
#include <chrono>
#include <expected>
#include <span>
#include <spdlog/spdlog.h>
#include <variant>

using asio::awaitable;
using asio::ip::tcp;
using asio::use_awaitable;
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
    asio::steady_timer timer{co_await asio::this_coro::executor};

    auto now = std::chrono::steady_clock::now();

    while (deadline > now) {
        timer.expires_at(deadline);
        co_await timer.async_wait(use_nothrow_awaitable);
        now = std::chrono::steady_clock::now();
    }

    spdlog::debug("Watchdog timer expired");
    co_return std::unexpected(asio::error::timed_out);
}

}  // namespace

namespace torrent::peer {

auto PeerConnection::send_data(std::span<const std::byte> buffer
) -> awaitable<std::expected<void, std::error_code>> {
    // Send the data
    auto [e, n] = co_await asio::async_write(socket, asio::buffer(buffer), use_nothrow_awaitable);

    if (e) {
        spdlog::error(
            "Failed to send data to {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            e.message()
        );
        co_return std::unexpected(e);
    }
    spdlog::debug("Sent {} bytes of data to peer {}:{}", n, peer_info.ip, peer_info.port);

    co_return std::expected<void, std::error_code>{};
}

auto PeerConnection::receive_data(std::span<std::byte> buffer
) -> awaitable<std::expected<void, std::error_code>> {
    // Receive the data
    auto [e, n] = co_await asio::async_read(socket, asio::buffer(buffer), use_nothrow_awaitable);

    if (e) {
        spdlog::error(
            "Failed to receive data from {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            e.message()
        );
        co_return std::unexpected(e);
    }
    spdlog::debug("Received {} bytes of data from peer {}:{}", n, peer_info.ip, peer_info.port);

    co_return std::expected<void, std::error_code>{};
}

auto PeerConnection::receive_handshake()
    -> awaitable<std::expected<crypto::Sha1, std::error_code>> {
    message::HandshakeMessage handshake_message;

    spdlog::debug("Waiting for handshake message from peer {}:{}", peer_info.ip, peer_info.port);

    std::chrono::steady_clock::time_point deadline{
        std::chrono::steady_clock::now() + duration::HANDSHAKE_TIMEOUT
    };

    // Receive the handshake message
    auto result = co_await (receive_data(handshake_message) || watchdog(deadline));
    std::expected<void, std::error_code> handshake_result;

    std::visit(
        visitor{[&handshake_result](const std::expected<void, std::error_code>& res) {
            handshake_result = res;
        }},
        result
    );

    if (!handshake_result.has_value()) {
        co_return std::unexpected(handshake_result.error());
    }

    // Parse the handshake message
    auto info_hash = message::parse_handshake_message(handshake_message);

    if (auto info_hash = message::parse_handshake_message(handshake_message);
        !info_hash.has_value()) {
        co_return std::unexpected(std::error_code{});
    } else {
        co_return info_hash.value();
    }
}

auto PeerConnection::send_handshake(const message::HandshakeMessage& handshake_message
) -> awaitable<std::expected<void, std::error_code>> {
    spdlog::debug("Sending handshake message to peer {}:{}", peer_info.ip, peer_info.port);

    std::chrono::steady_clock::time_point deadline{
        std::chrono::steady_clock::now() + duration::HANDSHAKE_TIMEOUT
    };

    auto result = co_await (send_data(handshake_message) || watchdog(deadline));
    std::expected<void, std::error_code> handshake_result;

    std::visit(
        visitor{[&handshake_result](const std::expected<void, std::error_code>& res) {
            handshake_result = res;
        }},
        result
    );

    co_return handshake_result;
}

auto PeerConnection::establish_connection() -> awaitable<std::expected<void, std::error_code>> {
    spdlog::debug("Establishing connection with peer {}:{}", peer_info.ip, peer_info.port);

    // Resolve the peer endpoint
    tcp::endpoint peer_endpoint =
        *tcp::resolver(peer_conn_ctx).resolve(peer_info.ip, std::to_string(peer_info.port));

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

auto PeerConnection::send_message(std::span<const std::byte> message
) -> asio::awaitable<std::expected<void, std::error_code>> {
    std::chrono::steady_clock::time_point deadline{
        std::chrono::steady_clock::now() + duration::SEND_MSG_TIMEOUT
    };

    auto result = co_await (send_data(message) || watchdog(deadline));

    std::expected<void, std::error_code> send_result;

    std::visit(
        visitor{[&send_result](const std::expected<void, std::error_code>& res) {
            send_result = res;
        }},
        result
    );

    co_return send_result;
}

awaitable<void> PeerConnection::connect(
    const message::HandshakeMessage& handshake_message, const crypto::Sha1& info_hash
) {
    // Set the state to connecting
    state = PeerState::CONNECTING;

    // Connect to the peer

    if (auto res = co_await establish_connection(); !res.has_value()) {
        spdlog::debug(
            "Failed to connect to peer {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            res.error().message()
        );
        // state =
        //     res.error() == asio::error::timed_out ? PeerState::TIMED_OUT :
        //     PeerState::DISCONNECTED;
        state = PeerState::DISCONNECTED;
        co_return;
    }

    // Send the handshake message

    if (auto res = co_await send_handshake(handshake_message); !res.has_value()) {
        spdlog::debug(
            "Failed to send handshake message to peer {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            res.error().message()
        );
        // state =
        //     res.error() == asio::error::timed_out ? PeerState::TIMED_OUT :
        //     PeerState::DISCONNECTED;
        state = PeerState::DISCONNECTED;
        co_return;
    }

    // Receive the handshake message

    if (auto res = co_await receive_handshake(); !res.has_value()) {
        spdlog::debug(
            "Failed to receive handshake message from peer {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            res.error().message()
        );
        // state =
        //     res.error() == asio::error::timed_out ? PeerState::TIMED_OUT :
        //     PeerState::DISCONNECTED;
        state = PeerState::DISCONNECTED;
        co_return;
    } else if (res.value() != info_hash) {
        spdlog::debug(
            "Received invalid handshake message from peer {}:{}", peer_info.ip, peer_info.port
        );
        state = PeerState::DISCONNECTED;
        co_return;
    }

    // Set the state to connected
    state = PeerState::CONNECTED;
    spdlog::debug("Successfully connected to peer {}:{}", peer_info.ip, peer_info.port);

    // Send interested message

    std::array<std::byte, 5> interested_message;

    message::create_interested_message(interested_message);
    if (auto res = co_await send_message(interested_message); !res.has_value()) {
        spdlog::debug(
            "Failed to send interested message to peer {}:{} with error:\n{}",
            peer_info.ip,
            peer_info.port,
            res.error().message()
        );
        // state =
        //     res.error() == asio::error::timed_out ? PeerState::TIMED_OUT :
        //     PeerState::DISCONNECTED;
        state = PeerState::DISCONNECTED;
        co_return;
    }

    co_return;

    // TODO: Also send bitfield message after implementing upload capabilities
}
}  // namespace torrent::peer
