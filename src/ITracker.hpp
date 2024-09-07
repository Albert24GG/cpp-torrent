#pragma once

#include "PeerInfo.hpp"

#include <chrono>
#include <optional>
#include <vector>

namespace torrent {

class ITracker {
    public:
        virtual ~ITracker() = default;

        /**
         * @brief Retrieve a list of peers from the tracker
         *
         * @param downloaded The number of bytes downloaded
         * @param uploaded The number of bytes uploaded
         * @return A list of peers if the request was successful, an empty optional otherwise
         */
        virtual auto retrieve_peers(size_t downloaded, size_t uploaded = 0)
            -> std::optional<std::vector<PeerInfo>> = 0;

        /**
         * @brief Get the interval at which the client should poll the tracker
         *
         * @return The interval
         */
        [[nodiscard]] virtual auto get_interval() const -> std::chrono::seconds = 0;
};

}  // namespace torrent
