#include "Utils.hpp"

#include "Logger.hpp"

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <system_error>

// Use the nothrow awaitable completion token to avoid exceptions
constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(asio::use_awaitable);

using namespace asio::experimental::awaitable_operators;

namespace torrent::utils {

auto watchdog(asio::chrono::steady_clock::time_point& deadline
) -> asio::awaitable<std::expected<void, std::error_code>> {
    asio::steady_timer timer{co_await asio::this_coro::executor};

    auto now = std::chrono::steady_clock::now();

    while (deadline > now) {
        timer.expires_at(deadline);
        co_await timer.async_wait(use_nothrow_awaitable);
        now = std::chrono::steady_clock::now();
    }

    LOG_DEBUG("Watchdog timer expired");
    co_return std::unexpected(asio::error::timed_out);
}

namespace tcp {

    auto send_data(
        asio::ip::tcp::socket&                        socket,
        std::span<const std::byte>                    buffer,
        std::optional<std::reference_wrapper<size_t>> bytes_sent
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        // Send the data
        auto [e, n] =
            co_await asio::async_write(socket, asio::buffer(buffer), use_nothrow_awaitable);

        if (bytes_sent.has_value()) {
            bytes_sent->get() = n;
        }

        if (e) {
            co_return std::unexpected(e);
        }

        co_return std::expected<void, std::error_code>{};
    }

    auto send_data_with_timeout(
        asio::ip::tcp::socket&                        socket,
        std::span<const std::byte>                    buffer,
        std::chrono::milliseconds                     timeout,
        std::optional<std::reference_wrapper<size_t>> bytes_sent
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + timeout};

        auto result = co_await (send_data(socket, buffer, bytes_sent) || watchdog(deadline));
        std::expected<void, std::error_code> return_res;

        std::visit(
            visitor{[&return_res](const std::expected<void, std::error_code>& res) {
                return_res = res;
            }},
            result
        );

        co_return return_res;
    }

    auto receive_data(
        asio::ip::tcp::socket&                        socket,
        std::span<std::byte>                          buffer,
        std::optional<std::reference_wrapper<size_t>> bytes_received
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        // Receive the data
        auto [e, n] =
            co_await asio::async_read(socket, asio::buffer(buffer), use_nothrow_awaitable);

        if (bytes_received.has_value()) {
            bytes_received->get() = n;
        }

        if (e) {
            co_return std::unexpected(e);
        }

        co_return std::expected<void, std::error_code>{};
    }

    auto receive_data_with_timeout(
        asio::ip::tcp::socket&                        socket,
        std::span<std::byte>                          buffer,
        std::chrono::milliseconds                     timeout,
        std::optional<std::reference_wrapper<size_t>> bytes_received
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + timeout};

        auto result = co_await (receive_data(socket, buffer, bytes_received) || watchdog(deadline));
        std::expected<void, std::error_code> return_res;

        std::visit(
            visitor{[&return_res](const std::expected<void, std::error_code>& res) {
                return_res = res;
            }},
            result
        );

        co_return return_res;
    }

}  // namespace tcp

namespace udp {

    auto send_data(
        asio::ip::udp::socket&                        socket,
        std::span<const std::byte>                    buffer,
        const asio::ip::udp::endpoint&                endpoint,
        std::optional<std::reference_wrapper<size_t>> bytes_sent
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        // Send the data
        auto [e, n] =
            co_await socket.async_send_to(asio::buffer(buffer), endpoint, use_nothrow_awaitable);

        if (bytes_sent.has_value()) {
            bytes_sent->get() = n;
        }

        if (e) {
            co_return std::unexpected(e);
        }

        co_return std::expected<void, std::error_code>{};
    }

    auto send_data_with_timeout(
        asio::ip::udp::socket&                        socket,
        std::span<const std::byte>                    buffer,
        const asio::ip::udp::endpoint&                endpoint,
        std::chrono::milliseconds                     timeout,
        std::optional<std::reference_wrapper<size_t>> bytes_sent
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + timeout};

        auto result =
            co_await (send_data(socket, buffer, endpoint, bytes_sent) || watchdog(deadline));
        std::expected<void, std::error_code> return_res;

        std::visit(
            visitor{[&return_res](const std::expected<void, std::error_code>& res) {
                return_res = res;
            }},
            result
        );

        co_return return_res;
    }

    auto receive_data(
        asio::ip::udp::socket&                        socket,
        std::span<std::byte>                          buffer,
        asio::ip::udp::endpoint&                      endpoint,
        std::optional<std::reference_wrapper<size_t>> bytes_received
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        // Receive the data
        auto [e, n] = co_await socket.async_receive_from(
            asio::buffer(buffer), endpoint, use_nothrow_awaitable
        );

        if (bytes_received.has_value()) {
            bytes_received->get() = n;
        }

        if (e) {
            co_return std::unexpected(e);
        }

        co_return std::expected<void, std::error_code>{};
    }

    auto receive_data_with_timeout(
        asio::ip::udp::socket&                        socket,
        std::span<std::byte>                          buffer,
        asio::ip::udp::endpoint&                      endpoint,
        std::chrono::milliseconds                     timeout,
        std::optional<std::reference_wrapper<size_t>> bytes_received
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + timeout};

        auto result =
            co_await (receive_data(socket, buffer, endpoint, bytes_received) || watchdog(deadline));
        std::expected<void, std::error_code> return_res;

        std::visit(
            visitor{[&return_res](const std::expected<void, std::error_code>& res) {
                return_res = res;
            }},
            result
        );

        co_return return_res;
    }

}  // namespace udp

}  // namespace torrent::utils
