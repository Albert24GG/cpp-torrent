#pragma once

#include <asio.hpp>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <functional>
#include <optional>
#include <random>
#include <span>
#include <system_error>

namespace torrent::utils {

/**
 * Convert the given value to big endian
 *
 * @param value  the value to convert
 * @return the value in big endian
 */
constexpr auto host_to_network_order(auto value) {
    if constexpr (std::endian::native == std::endian::big) {
        return value;
    } else {
        return std::byteswap(value);
    }
}

/**
 * Convert the given value to host endian
 *
 * @param value  the value to convert
 * @return the value in host endian
 */
constexpr auto network_to_host_order(auto value) {
    return host_to_network_order(value);
}

/**
 * Calculate the ceiling of the division of the given integers
 *
 * @param divident the divident
 * @param divisor  the divisor
 * @return the ceiling of the division
 */
constexpr auto ceil_div(std::integral auto divident, std::integral auto divisor) {
    return (divident / divisor) + (divident % divisor != 0);
}

/**
 * Calculate the next aligned value of the given value
 *
 * @param value      the value to align
 * @param alignment  the alignment
 * @return the next aligned value
 */
template <typename T, typename U = size_t>
    requires std::integral<T> && std::integral<U>
constexpr auto next_aligned(T value, U alignment = alignof(std::max_align_t)) {
    return (value + (alignment - 1)) & ~(alignment - 1);
}

template <typename T>
    requires std::integral<T>
constexpr auto generate_random(
    T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max()
) {
    static std::random_device        rd;
    static std::mt19937              gen(rd());
    std::uniform_int_distribution<T> dist(min, max);

    return dist(gen);
}

/** A visitor to use with std::visit on a variant */
template <typename... Callable>
struct visitor : Callable... {
        using Callable::operator()...;
};

/**
 * Create a timer to watch for the given deadline
 * This function is intended to be used in conjuntion with the awaitable operators
 *
 * @param deadline  the deadline to watch for
 * @return the result of the operation: only returns an error code when the deadline is reached
 */
auto watchdog(asio::chrono::steady_clock::time_point& deadline
) -> asio::awaitable<std::expected<void, std::error_code>>;

namespace tcp {

    /**
     * Send the given data to the socket
     *
     * @param socket  the socket to send the data to
     * @param buffer  the data to send
     * @return the result of the operation: void if successful, error code otherwise
     */
    auto send_data(asio::ip::tcp::socket& socket, std::span<const std::byte> buffer)
        -> asio::awaitable<std::expected<void, std::error_code>>;

    /**
     * Send the given data to the socket with a timeout
     *
     * @param socket  the socket to send the data to
     * @param buffer  the data to send
     * @param timeout the timeout for the operation
     * @return the result of the operation: void if successful, error code if the operation
     * failed or timed out
     */
    auto send_data_with_timeout(
        asio::ip::tcp::socket&     socket,
        std::span<const std::byte> buffer,
        std::chrono::milliseconds  timeout
    ) -> asio::awaitable<std::expected<void, std::error_code>>;

    /**
     * Receive data from the socket
     *
     * @param socket  the socket to receive the data from
     * @param buffer  the buffer to store the received data
     * @return the result of the operation: void if successful, error code otherwise
     */
    auto receive_data(asio::ip::tcp::socket& socket, std::span<std::byte> buffer)
        -> asio::awaitable<std::expected<void, std::error_code>>;

    /**
     * Receive data from the socket with a timeout
     *
     * @param socket  the socket to receive the data from
     * @param buffer  the buffer to store the received data
     * @param timeout the timeout for the operation
     * @return the result of the operation: void if successful, error code if the operation
     * failed or timed out
     */
    auto receive_data_with_timeout(
        asio::ip::tcp::socket&    socket,
        std::span<std::byte>      buffer,
        std::chrono::milliseconds timeout
    ) -> asio::awaitable<std::expected<void, std::error_code>>;

}  // namespace tcp

namespace udp {

    /**
     * Send the given data to the socket
     *
     * @param socket    the socket to be used for sending the data
     * @param buffer    the data to send
     * @param endpoint  the endpoint to send the data to
     * @param bytes_send reference to a variable to store the number of bytes sent
     * @return the result of the operation: void if successful, error code otherwise
     */
    auto send_data(
        asio::ip::udp::socket&                        socket,
        std::span<const std::byte>                    buffer,
        const asio::ip::udp::endpoint&                endpoint,
        std::optional<std::reference_wrapper<size_t>> bytes_sent = std::nullopt
    ) -> asio::awaitable<std::expected<void, std::error_code>>;

    /**
     * Send the given data to the socket with a timeout
     *
     * @param socket    the socket to be used for sending the data
     * @param buffer    the data to send
     * @param endpoint  the endpoint to send the data to
     * @param timeout   the timeout for the operation
     * @param bytes_send reference to a variable to store the number of bytes sent
     * @return the result of the operation: void if successful, error code if the operation
     * failed or timed out
     */
    auto send_data_with_timeout(
        asio::ip::udp::socket&                        socket,
        std::span<const std::byte>                    buffer,
        const asio::ip::udp::endpoint&                endpoint,
        std::chrono::milliseconds                     timeout,
        std::optional<std::reference_wrapper<size_t>> bytes_sent = std::nullopt
    ) -> asio::awaitable<std::expected<void, std::error_code>>;

    /**
     * Receive data from the socket
     *
     * @param socket    the socket to be used for receiving the data
     * @param buffer    the buffer to store the received data
     * @param endpoint  the endpoint to receive the data from
     * @param bytes_received reference to a variable to store the number of bytes received
     * @return the result of the operation: void if successful, error code otherwise
     */
    auto receive_data(
        asio::ip::udp::socket&                        socket,
        std::span<std::byte>                          buffer,
        asio::ip::udp::endpoint&                      endpoint,
        std::optional<std::reference_wrapper<size_t>> bytes_received = std::nullopt
    ) -> asio::awaitable<std::expected<void, std::error_code>>;

    /**
     * Receive data from the socket with a timeout
     *
     * @param socket    the socket to be used for receiving the data
     * @param buffer    the buffer to store the received data
     * @param endpoint  the endpoint to receive the data from
     * @param timeout   the timeout for the operation
     * @param bytes_received reference to a variable to store the number of bytes received
     * @return the result of the operation: void if successful, error code if the operation
     * failed or timed out
     */
    auto receive_data_with_timeout(
        asio::ip::udp::socket&                        socket,
        std::span<std::byte>                          buffer,
        asio::ip::udp::endpoint&                      endpoint,
        std::chrono::milliseconds                     timeout,
        std::optional<std::reference_wrapper<size_t>> bytes_received = std::nullopt
    ) -> asio::awaitable<std::expected<void, std::error_code>>;

}  // namespace udp

}  // namespace torrent::utils
