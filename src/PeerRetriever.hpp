#pragma once

#include "Crypto.hpp"
#include "Error.hpp"
#include "ITracker.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace torrent {

class PeerRetriever {
    public:
        PeerRetriever(
            std::string                           announce,
            std::vector<std::vector<std::string>> announce_list,
            crypto::Sha1                          info_hash,
            std::string                           client_id,
            uint16_t                              client_port,
            size_t                                torrent_size
        )
            : announce_list_(std::move(announce_list)),
              info_hash_(info_hash),
              client_id_(std::move(client_id)),
              client_port_(client_port),
              torrent_size_(torrent_size) {
            if (client_id_.size() != 20) {
                err::throw_with_trace("Client ID must be 20 bytes long");
            }

            // Add the announce url to the announce list if it is not already present
            if (announce_list_.empty()) {
                announce_list_.push_back(std::vector<std::string>{std::move(announce)});
            } else if (std::ranges::find(announce_list_[0], announce) == announce_list_[0].end()) {
                announce_list_[0].insert(announce_list_[0].begin(), std::move(announce));
            }
        }

        /**
         * @brief Retrieves a list of peers from the tracker
         *
         * @param downloaded The number of bytes downloaded
         * @param uploaded The number of bytes uploaded
         * @return A list of peers if the request was successful, an empty optional otherwise
         */
        auto retrieve_peers(size_t downloaded, size_t uploaded = 0)
            -> std::optional<std::vector<PeerInfo>>;

        /**
         * @brief Get the interval at which the client should poll the tracker
         *
         * @return The interval
         */
        [[nodiscard]] auto get_interval() const -> std::chrono::seconds {
            return cur_tracker_ != nullptr ? cur_tracker_->get_interval() : std::chrono::seconds(0);
        }

    private:
        std::unique_ptr<ITracker>             cur_tracker_;
        std::vector<std::vector<std::string>> announce_list_;
        crypto::Sha1                          info_hash_;
        std::string                           client_id_;
        uint16_t                              client_port_;
        size_t                                torrent_size_;
};

}  // namespace torrent
