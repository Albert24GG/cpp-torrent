#include "MockUdpTracker.hpp"

#include "Utils.hpp"

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <cstddef>
#include <ranges>
#include <span>

// Use the nothrow awaitable completion token to avoid exceptions
constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(asio::use_awaitable);

using asio::ip::udp;
namespace utils = torrent::utils;

std::span<std::byte> MockUdpTracker::get_connect_response(
    uint32_t transaction_id, uint64_t connection_id
) {
    static std::array<std::byte, 16> response_buffer;

    *reinterpret_cast<uint32_t*>(response_buffer.data()) = 0;

    *reinterpret_cast<uint32_t*>(response_buffer.data() + 4) =
        utils::host_to_network_order(transaction_id);

    *reinterpret_cast<uint64_t*>(response_buffer.data() + 8) =
        utils::host_to_network_order(connection_id);

    return response_buffer;
}

std::span<std::byte> MockUdpTracker::get_announce_response(
    uint32_t transaction_id, uint32_t num_want
) {
    static std::vector<std::byte> response_buffer(20 + 6 * peers_.size());

    *reinterpret_cast<uint32_t*>(response_buffer.data()) =
        utils::host_to_network_order(static_cast<uint32_t>(1));

    *reinterpret_cast<uint32_t*>(response_buffer.data() + 4) =
        utils::host_to_network_order(transaction_id);

    *reinterpret_cast<uint32_t*>(response_buffer.data() + 8) =
        utils::host_to_network_order(interval_);

    *reinterpret_cast<uint32_t*>(response_buffer.data() + 12) = 0;

    *reinterpret_cast<uint32_t*>(response_buffer.data() + 16) =
        utils::host_to_network_order(static_cast<uint32_t>(peers_.size()));

    auto str_to_byte = [](std::string_view str) -> std::byte {
        uint8_t value{};
        // ignore the error code
        std::from_chars(str.data(), str.data() + str.size(), value);
        return static_cast<std::byte>(value);
    };

    for (auto const& [index, peer] : std::views::enumerate(peers_)) {
        {
            std::string ip{peer.ip};
            uint16_t    port{peer.port};

            // get the ip address numbers and append them to the peer list
            std::ranges::copy(
                ip | std::views::split('.') | std::views::transform([&str_to_byte](auto&& range) {
                    return str_to_byte(std::string_view(range.begin(), range.end()));
                }),
                std::ranges::begin(response_buffer | std::views::drop(20 + 6 * index))
            );

            // append the port number to the peer list
            *reinterpret_cast<uint16_t*>(response_buffer.data() + 24 + 6 * index) =
                utils::host_to_network_order(port);
        }
    }
    return {
        response_buffer.begin(), 20 + 6 * std::min(peers_.size(), static_cast<size_t>(num_want))
    };
}

void MockUdpTracker::start_server() {
    asio::co_spawn(
        server_context_,
        [this]() -> asio::awaitable<void> {
            for (;;) {
                udp::endpoint remote_endpoint;

                static std::array<std::byte, 98> receive_buffer;
                size_t                           received_bytes{};

                if (auto res = co_await utils::udp::receive_data(
                        socket_, receive_buffer, remote_endpoint, std::ref(received_bytes)
                    );
                    !res.has_value()) {
                    continue;
                }

                if (received_bytes < 16) {
                    continue;
                }

                uint32_t received_action{utils::network_to_host_order(
                    *reinterpret_cast<uint32_t*>(receive_buffer.data() + 8)
                )};

                std::span<std::byte> response_buffer;

                switch (received_action) {
                    case 0: {  // connect
                        uint64_t received_protocol_id{utils::network_to_host_order(
                            *reinterpret_cast<uint64_t*>(receive_buffer.data())
                        )};

                        if (received_protocol_id != static_cast<uint64_t>(0x41727101980)) {
                            break;
                        }

                        uint32_t received_transaction_id{utils::network_to_host_order(
                            *reinterpret_cast<uint32_t*>(receive_buffer.data() + 12)
                        )};

                        connection_id_ = utils::generate_random<uint64_t>();

                        response_buffer =
                            get_connect_response(received_transaction_id, connection_id_);

                        break;
                    }
                    case 1: {  // announce
                        if (received_bytes < 98) {
                            break;
                        }
                        uint64_t received_connection_id{utils::network_to_host_order(
                            *reinterpret_cast<uint64_t*>(receive_buffer.data())
                        )};

                        uint32_t received_num_want{utils::network_to_host_order(
                            *reinterpret_cast<uint32_t*>(receive_buffer.data() + 92)
                        )};

                        torrent::crypto::Sha1 received_info_hash{
                            torrent::crypto::Sha1::from_raw_data(
                                reinterpret_cast<uint8_t*>(receive_buffer.data() + 16)
                            )
                        };

                        if (received_connection_id != connection_id_ ||
                            received_info_hash != info_hash_ || received_num_want == 0) {
                            break;
                        }

                        uint32_t received_transaction_id{utils::network_to_host_order(
                            *reinterpret_cast<uint32_t*>(receive_buffer.data() + 12)
                        )};

                        response_buffer =
                            get_announce_response(received_transaction_id, received_num_want);

                        break;
                    }
                }

                if (response_buffer.empty()) {
                    continue;
                }

                co_await utils::udp::send_data(socket_, response_buffer, remote_endpoint);
            }
        },
        asio::detached
    );

    server_thread_ = std::jthread([this]() { server_context_.run(); });
}
