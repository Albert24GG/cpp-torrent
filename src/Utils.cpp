#include "Utils.hpp"

#include "Logger.hpp"

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <expected>
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

    auto send_data(asio::ip::tcp::socket& socket, std::span<const std::byte> buffer)
        -> asio::awaitable<std::expected<void, std::error_code>> {
        // Send the data
        auto [e, n] =
            co_await asio::async_write(socket, asio::buffer(buffer), use_nothrow_awaitable);

        if (e) {
            co_return std::unexpected(e);
        }

        co_return std::expected<void, std::error_code>{};
    }

    auto send_data_with_timeout(
        asio::ip::tcp::socket&     socket,
        std::span<const std::byte> buffer,
        std::chrono::milliseconds  timeout
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + timeout};

        auto result = co_await (send_data(socket, buffer) || watchdog(deadline));
        std::expected<void, std::error_code> return_res;

        std::visit(
            visitor{[&return_res](const std::expected<void, std::error_code>& res) {
                return_res = res;
            }},
            result
        );

        co_return return_res;
    }

    auto receive_data(asio::ip::tcp::socket& socket, std::span<std::byte> buffer)
        -> asio::awaitable<std::expected<void, std::error_code>> {
        // Receive the data
        auto [e, n] =
            co_await asio::async_read(socket, asio::buffer(buffer), use_nothrow_awaitable);

        if (e) {
            co_return std::unexpected(e);
        }

        co_return std::expected<void, std::error_code>{};
    }

    auto receive_data_with_timeout(
        asio::ip::tcp::socket&    socket,
        std::span<std::byte>      buffer,
        std::chrono::milliseconds timeout
    ) -> asio::awaitable<std::expected<void, std::error_code>> {
        std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::now() + timeout};

        auto result = co_await (receive_data(socket, buffer) || watchdog(deadline));
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

}  // namespace torrent::utils
