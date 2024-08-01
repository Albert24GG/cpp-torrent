#include "Crypto.hpp"
#include "MockHttpTracker.hpp"
#include "PeerInfo.hpp"
#include "Tracker.hpp"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <thread>

TEST_CASE("Tracker: retrieve_peers", "[Tracker]") {
    static constexpr std::array<torrent::PeerInfo, 5> peers{
        {{"192.168.0.1", 6'881},
         {"192.168.0.2", 6'882},
         {"192.168.0.3", 6'883},
         {"192.168.0.4", 6'884},
         {"192.168.0.5", 6'885}}
    };
    static constexpr std::array<uint8_t, 20> info_hash_arr{
        {0xf9, 0x7f, 0x10, 0xce, 0xf3, 0x26, 0xaf, 0xcb, 0xf2, 0x7b,
         0xc7, 0x35, 0xe9, 0x85, 0x57, 0xe8, 0x4d, 0x33, 0xb9, 0xfe}
    };
    torrent::crypto::Sha1 info_hash{torrent::crypto::Sha1::from_raw_data(info_hash_arr)};

    MockHttpTracker tracker{peers, info_hash, 1};

    std::jthread server_thread{[&tracker] { tracker.start(); }};

    tracker.wait_until_ready();

    torrent::Tracker torrent_tracker{
        "http://localhost:8080/announce", info_hash, "client_id", 6'886, 1'000
    };

    auto retrieved_peers = torrent_tracker.retrieve_peers(0);

    REQUIRE(retrieved_peers.has_value());

    REQUIRE(retrieved_peers->size() == 5);

    REQUIRE(std::equal(retrieved_peers->begin(), retrieved_peers->end(), peers.begin()));

    tracker.stop();
}
