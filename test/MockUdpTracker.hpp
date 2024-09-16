#pragma once

#include "Crypto.hpp"
#include "PeerInfo.hpp"

#include <asio.hpp>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

class MockUdpTracker {
    public:
        MockUdpTracker(
            std::span<const torrent::PeerInfo> peers_span,
            torrent::crypto::Sha1              info_hash,
            uint32_t                           interval,
            uint16_t                           port
        )
            : peers_{peers_span | std::ranges::to<decltype(peers_)>()},
              info_hash_{info_hash},
              interval_{interval},
              socket_{server_context_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)} {}

        MockUdpTracker(const MockUdpTracker&)            = delete;
        MockUdpTracker& operator=(const MockUdpTracker&) = delete;
        MockUdpTracker(MockUdpTracker&&)                 = delete;
        MockUdpTracker& operator=(MockUdpTracker&&)      = delete;

        void start_server();
        void stop_server() {
            socket_.close();
            server_context_.stop();
        };

    private:
        std::span<std::byte> get_connect_response(uint32_t transaction_id, uint64_t connection_id);

        std::span<std::byte> get_announce_response(uint32_t transaction_id, uint32_t num_want);

        uint32_t                       interval_;
        std::vector<torrent::PeerInfo> peers_;
        torrent::crypto::Sha1          info_hash_;

        asio::io_context      server_context_;
        asio::ip::udp::socket socket_;
        std::jthread          server_thread_;

        uint64_t connection_id_{};
};
