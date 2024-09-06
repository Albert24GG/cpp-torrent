#pragma once

#include "PeerInfo.hpp"

#include <optional>
#include <vector>

namespace torrent {

class ITracker {
    public:
        virtual auto retrieve_peers(size_t downloaded, size_t uploaded = 0)
            -> std::optional<std::vector<PeerInfo>> = 0;
        virtual ~ITracker()                         = default;
};

}  // namespace torrent
