#include "MockHttpTracker.hpp"

#include "Crypto.hpp"
#include "PeerInfo.hpp"

#include <bit>
#include <charconv>
#include <format>
#include <httplib.h>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

namespace {

std::string get_compact_peer_list(std::span<const torrent::PeerInfo>& peers) {
    std::string peer_list;
    peer_list.reserve(6 * peers.size());

    auto str_to_uint8 = [](std::string_view str) -> uint8_t {
        uint8_t value{};
        // ignore the error code
        std::from_chars(str.data(), str.data() + str.size(), value);
        return value;
    };

    for (const auto& peer : peers) {
        std::string ip{peer.ip};
        uint16_t    port{peer.port};

        // get the ip address numbers and append them to the peer list
        std::ranges::copy(
            ip | std::views::split('.') | std::views::transform([&str_to_uint8](auto&& range) {
                return str_to_uint8(std::string_view(range.begin(), range.end()));
            }),
            std::back_inserter(peer_list)
        );

        // convert the port number to network byte order
        if constexpr (std::endian::native == std::endian::little) {
            port = std::byteswap(port);
        }

        // append the port number to the peer list
        std::ranges::copy(
            std::span(reinterpret_cast<uint8_t*>(&port), 2), std::back_inserter(peer_list)
        );
    }
    return peer_list;
}

std::string build_response(std::span<const torrent::PeerInfo>& peers, uint64_t interval) {
    std::string compact_peer_list{get_compact_peer_list(peers)};
    return std::format(
        "d8:intervali{}e5:peers{}:{}e", interval, compact_peer_list.size(), compact_peer_list
    );
}

}  // namespace

MockHttpTracker::MockHttpTracker(
    std::span<const torrent::PeerInfo> peers, torrent::crypto::Sha1 info_hash, uint64_t interval
) {
    server.Get(
        "/announce",
        [info_hash, response = build_response(peers, interval)](
            const httplib::Request& req, httplib::Response& res
        ) {
            auto hash_str = req.get_param_value("info_hash");

            if (hash_str.size() != 20) {
                res.status = httplib::StatusCode::BadRequest_400;
                return;
            }

            torrent::crypto::Sha1 hash{
                torrent::crypto::Sha1::from_raw_data(reinterpret_cast<uint8_t*>(hash_str.data()))
            };

            if (hash != info_hash) {
                res.status = httplib::StatusCode::BadRequest_400;
            } else {
                res.set_content(response, "text/plain");
            }
        }
    );
}
