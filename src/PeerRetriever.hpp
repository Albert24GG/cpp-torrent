#pragma once

#include "Crypto.hpp"
#include "Error.hpp"
#include "ITracker.hpp"

#include <algorithm>
#include <iostream>
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
            : announce_list(std::move(announce_list)),
              info_hash(info_hash),
              client_id(std::move(client_id)),
              client_port(client_port),
              torrent_size(torrent_size) {
            if (this->client_id.size() != 20) {
                err::throw_with_trace("Client ID must be 20 bytes long");
            }

            // Add the announce url to the announce list if it is not already present
            if (this->announce_list.empty()) {
                this->announce_list.push_back(std::vector<std::string>{std::move(announce)});
            } else if (std::ranges::find(this->announce_list[0], announce) ==
                       this->announce_list[0].end()) {
                this->announce_list[0].insert(this->announce_list[0].begin(), std::move(announce));
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

    private:
        std::unique_ptr<ITracker>             cur_tracker;
        std::vector<std::vector<std::string>> announce_list;
        crypto::Sha1                          info_hash;
        std::string                           client_id;
        uint16_t                              client_port;
        size_t                                torrent_size;
};

}  // namespace torrent
