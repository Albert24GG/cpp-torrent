#pragma once

#include "Bencode.hpp"
#include "Crypto.hpp"
#include "Error.hpp"
#include "ITracker.hpp"
#include "PeerInfo.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace torrent {

inline constexpr auto TRACKER_TIMEOUT{std::chrono::seconds(60)};

class HttpTracker final : public ITracker {
    public:
        HttpTracker(
            std::string  announce,
            crypto::Sha1 info_hash,
            std::string  client_id,
            uint16_t     client_port,
            size_t       torrent_size
        )
            : announce_(std::move(announce)),
              info_hash_(info_hash),
              torr_client_id_(std::move(client_id)),
              torr_client_port_(client_port),
              torrent_size_(torrent_size) {
            if (torr_client_id_.size() != 20) {
                err::throw_with_trace("Client ID must be 20 bytes long");
            }
        }

        /**
         * @brief Retrieves a list of peers from the tracker
         *
         * @param downloaded The number of bytes downloaded
         * @param uploaded The number of bytes uploaded
         * @return A list of peers if the request was successful, an empty optional otherwise
         */
        std::optional<std::vector<PeerInfo>> retrieve_peers(size_t downloaded, size_t uploaded = 0)
            override;

    private:
        /**
         * @brief Update the interval using the value in the response dictionary
         *
         * @param response_dict The response dictionary
         */
        void update_interval(const Bencode::BencodeDict& response_dict);

        std::string          announce_;
        crypto::Sha1         info_hash_;
        std::string          torr_client_id_;
        uint16_t             torr_client_port_;
        size_t               uploaded_{0};
        size_t               downloaded_{};
        size_t               torrent_size_{};
        bool                 compact_{true};
        std::chrono::seconds interval_{};
};

}  // namespace torrent
