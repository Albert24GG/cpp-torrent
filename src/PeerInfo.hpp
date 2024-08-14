#pragma once

#include <cstdint>
#include <string>
#include <tuple>

namespace torrent {

struct PeerInfo {
        std::string ip;
        uint16_t    port;

        bool operator<(const PeerInfo& other) const {
            return std::tie(ip, port) < std::tie(other.ip, other.port);
        }

        bool operator==(const PeerInfo&) const = default;
};

}  // namespace torrent

template <>
struct std::hash<torrent::PeerInfo> {
        std::size_t operator()(const torrent::PeerInfo& peer_info) const noexcept {
            return std::hash<std::string>{}(peer_info.ip) ^ std::hash<uint16_t>{}(peer_info.port);
        }
};
