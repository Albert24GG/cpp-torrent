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

        bool operator==(const PeerInfo& other) const {
            return std::tie(ip, port) == std::tie(other.ip, other.port);
        }
};

}  // namespace torrent
