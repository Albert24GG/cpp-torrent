#include "UdpTracker.hpp"

#include "Error.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <array>
#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <charconv>
#include <cstdint>
#include <expected>
#include <iostream>
#include <optional>
#include <ranges>
#include <regex>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Use the nothrow awaitable completion token to avoid exceptions
constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(asio::use_awaitable);

using namespace asio::experimental::awaitable_operators;
namespace this_coro = asio::this_coro;
using asio::awaitable;
using asio::ip::udp;

constexpr inline auto     UDP_TRACKER_TIMEOUT{std::chrono::seconds(60)};
constexpr inline uint32_t UDP_TRACKER_NUM_WANT{50U};

namespace {

auto extract_peers(std::span<std::byte> peers_buffer
) -> awaitable<std::expected<std::vector<torrent::PeerInfo>, std::error_code>> {
    if (peers_buffer.size() % 6 != 0) {
        co_return std::unexpected(std::make_error_code(std::errc::protocol_error));
    }

    std::vector<torrent::PeerInfo> peer_list;
    peer_list.reserve(peers_buffer.size() / 6);

    for (auto peer : peers_buffer | std::views::chunk(6)) {
        std::array<uint8_t, 4> ip{};
        uint16_t               port{};

        std::ranges::copy(
            peer | std::views::take(4), std::ranges::begin(std::as_writable_bytes<uint8_t, 4>(ip))
        );

        *reinterpret_cast<uint16_t*>(&port) = *reinterpret_cast<uint16_t*>(peer.data() + 4);

        // if the system is little endian, reverse the order of the bytes
        port = torrent::utils::network_to_host_order(port);

        peer_list.emplace_back(std::format("{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]), port);
    }

    LOG_INFO("Extracted {} peers from tracker response", peer_list.size());
    co_return peer_list;
}

}  // namespace

namespace torrent {

auto UdpTracker::send_connect_request(udp::socket& socket, const udp::endpoint& tracker_endpoint)
    -> awaitable<std::expected<uint64_t, std::error_code>> {
    static std::array<std::byte, 16> connect_buffer;

    // Set the magic numbe (protocol id)
    *reinterpret_cast<uint64_t*>(connect_buffer.data()) =
        utils::host_to_network_order(static_cast<uint64_t>(0x41727101980));

    // Set the action (0 for connect)
    *reinterpret_cast<uint32_t*>(connect_buffer.data() + 8) = 0;

    uint32_t transaction_id = utils::generate_random<uint32_t>();
    *reinterpret_cast<uint32_t*>(connect_buffer.data() + 12) =
        utils::host_to_network_order(transaction_id);

    // Send the connect request
    if (auto res = co_await utils::udp::send_data(socket, connect_buffer, tracker_endpoint);
        !res.has_value()) {
        co_return std::unexpected(res.error());
    }

    udp::endpoint sender_endpoint;

    if (auto res = co_await utils::udp::receive_data_with_timeout(
            socket, connect_buffer, sender_endpoint, UDP_TRACKER_TIMEOUT
        );
        !res.has_value()) {
        co_return std::unexpected(res.error());
    }

    uint32_t received_action{
        utils::network_to_host_order(*reinterpret_cast<uint32_t*>(connect_buffer.data()))
    };

    uint32_t received_transaction_id{
        utils::network_to_host_order(*reinterpret_cast<uint32_t*>(connect_buffer.data() + 4))
    };

    // Check if the response is valid
    if (received_action != 0 || received_transaction_id != transaction_id) {
        co_return std::unexpected(std::make_error_code(std::errc::protocol_error));
    }

    uint64_t connection_id{
        utils::network_to_host_order(*reinterpret_cast<uint64_t*>(connect_buffer.data() + 8))
    };
    co_return connection_id;
}

auto UdpTracker::send_announce_request(
    udp::socket& socket, const udp::endpoint& tracker_endpoint, uint64_t connection_id
) -> awaitable<std::expected<std::vector<PeerInfo>, std::error_code>> {
    // Create the announce buffer
    static std::vector<std::byte> announce_buffer(std::max(98U, 20 + 6 * UDP_TRACKER_NUM_WANT));

    *reinterpret_cast<uint64_t*>(announce_buffer.data()) =
        utils::host_to_network_order(connection_id);

    // Set the action (1 for announce)
    *reinterpret_cast<uint32_t*>(announce_buffer.data() + 8) =
        utils::host_to_network_order(static_cast<uint32_t>(1));

    uint32_t transaction_id{utils::generate_random<uint32_t>()};
    *reinterpret_cast<uint32_t*>(announce_buffer.data() + 12) =
        utils::host_to_network_order(transaction_id);

    // Set the info hash
    std::ranges::copy(
        std::as_bytes(info_hash.get()), std::ranges::begin(announce_buffer | std::views::drop(16))
    );

    // Set the peer id
    std::ranges::copy(
        std::as_bytes(std::span{torr_client_id}),
        std::ranges::begin(announce_buffer | std::views::drop(36))
    );

    *reinterpret_cast<uint64_t*>(announce_buffer.data() + 56) =
        utils::host_to_network_order(downloaded);

    // Set the left bytes
    *reinterpret_cast<uint64_t*>(announce_buffer.data() + 64) =
        utils::host_to_network_order(torrent_size - downloaded);

    *reinterpret_cast<uint64_t*>(announce_buffer.data() + 72) =
        utils::host_to_network_order(uploaded);

    // Set the event (0 for none)
    *reinterpret_cast<uint32_t*>(announce_buffer.data() + 80) = 0;

    // Set the IP address (0 for default)
    *reinterpret_cast<uint32_t*>(announce_buffer.data() + 84) = 0;

    // Set the key (0 for default)
    *reinterpret_cast<uint32_t*>(announce_buffer.data() + 88) = 0;

    *reinterpret_cast<int32_t*>(announce_buffer.data() + 92) =
        utils::host_to_network_order(static_cast<uint32_t>(UDP_TRACKER_NUM_WANT));

    *reinterpret_cast<uint16_t*>(announce_buffer.data() + 96) =
        utils::host_to_network_order(torr_client_port);

    // Send the announce request
    if (auto res = co_await utils::udp::send_data(socket, announce_buffer, tracker_endpoint);
        !res.has_value()) {
        co_return std::unexpected(res.error());
    }

    udp::endpoint sender_endpoint;

    size_t bytes_received{0};

    // Receive the announce response
    if (auto res = co_await utils::udp::receive_data_with_timeout(
            socket, announce_buffer, sender_endpoint, UDP_TRACKER_TIMEOUT, std::ref(bytes_received)
        );
        !res.has_value()) {
        co_return std::unexpected(res.error());
    }

    if (bytes_received < 20) {
        co_return std::unexpected(std::make_error_code(std::errc::protocol_error));
    }

    uint32_t received_action{
        utils::network_to_host_order(*reinterpret_cast<uint32_t*>(announce_buffer.data()))
    };
    uint32_t received_transaction_id{
        utils::network_to_host_order(*reinterpret_cast<uint32_t*>(announce_buffer.data() + 4))
    };

    if (received_action != 1 || received_transaction_id != transaction_id) {
        co_return std::unexpected(std::make_error_code(std::errc::protocol_error));
    }

    std::span<std::byte> peers_buffer(
        std::ranges::begin(announce_buffer | std::views::drop(20)), bytes_received - 20
    );

    co_return co_await extract_peers(peers_buffer);
}

auto UdpTracker::retrieve_peers(size_t downloaded, size_t uploaded)
    -> std::optional<std::vector<PeerInfo>> {
    asio::io_context                     io_context;
    std::optional<std::vector<PeerInfo>> extracted_peers;

    this->downloaded = downloaded;
    this->uploaded   = uploaded;

    asio::co_spawn(
        io_context,
        [this, &extracted_peers]() -> awaitable<void> {
            udp::socket socket{co_await this_coro::executor};

            auto [error, result_endpoints] =
                co_await udp::resolver{co_await this_coro::executor}.async_resolve(
                    host, std::to_string(port), use_nothrow_awaitable
                );

            if (error) {
                LOG_ERROR(
                    "Failed to resolve udp host {}:{} with error : {}", host, port, error.message()
                );
                co_return;
            }

            udp::endpoint tracker_endpoint = *result_endpoints.begin();
            socket.open(udp::v4());

            uint64_t connection_id{};

            if (auto res = co_await send_connect_request(socket, tracker_endpoint);
                !res.has_value()) {
                LOG_ERROR(
                    "Failed to connect to tracker {}:{} with error : {}",
                    host,
                    port,
                    res.error().message()
                );
                co_return;
            } else {
                connection_id = *res;
            }

            if (auto res = co_await send_announce_request(socket, tracker_endpoint, connection_id);
                !res.has_value()) {
                LOG_ERROR(
                    "Failed to retrieve peers from udp tracker {}:{} with error : {}",
                    host,
                    port,
                    res.error().message()
                );
                co_return;
            } else {
                extracted_peers = *res;
            }
        },
        asio::detached
    );

    io_context.run();
    return extracted_peers;
}

auto UdpTracker::extract_url_info(const std::string& url) -> std::pair<std::string, uint16_t> {
    std::regex  url_regex{R"(udp://(.+?):(\d+)/?.*)"};
    std::smatch match;

    if (std::regex_search(url, match, url_regex) && match.size() == 3) {
        std::string host{match[1].str()};
        std::string port_str{match[2].str()};
        uint16_t    port{};

        if (std::from_chars(port_str.data(), port_str.data() + port_str.size(), port).ec ==
            std::errc{}) {
            return std::make_pair(host, port);
        } else {
            err::throw_with_trace("Invalid udp port number");
        }

    } else {
        err::throw_with_trace("Invalid udp URL format");
    }
}

}  // namespace torrent
