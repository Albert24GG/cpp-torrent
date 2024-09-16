#pragma once

#include "Crypto.hpp"
#include "PeerInfo.hpp"

#include <httplib.h>
#include <span>
#include <string>
#include <thread>

class MockHttpTracker {
    public:
        MockHttpTracker(
            std::span<const torrent::PeerInfo> peers,
            torrent::crypto::Sha1              info_hash,
            uint64_t                           interval
        );

        MockHttpTracker(const MockHttpTracker&)            = delete;
        MockHttpTracker& operator=(const MockHttpTracker&) = delete;
        MockHttpTracker(MockHttpTracker&&)                 = delete;
        MockHttpTracker& operator=(MockHttpTracker&&)      = delete;

        ~MockHttpTracker() { server_.stop(); }

        void start() { server_.listen("localhost", 8'080); }
        void wait_until_ready() { server_.wait_until_ready(); }
        void stop() { server_.stop(); }

    private:
        httplib::Server server_;
        std::jthread    server_thread_;
};
