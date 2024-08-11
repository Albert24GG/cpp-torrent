#pragma once

#include "Bencode.hpp"
#include "Crypto.hpp"
#include "PeerInfo.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace torrent {

static constexpr auto TRACKER_TIMEOUT{std::chrono::seconds(60)};

class Tracker {
    public:
        Tracker(
            std::string  announce,
            crypto::Sha1 info_hash,
            std::string  client_id,
            uint16_t     client_port,
            size_t       torrent_size
        )
            : announce(std::move(announce)),
              info_hash(info_hash),
              torr_client_id(std::move(client_id)),
              torr_client_port(client_port),
              torrent_size(torrent_size) {}

        std::optional<std::vector<PeerInfo>> retrieve_peers(size_t downloaded, size_t uploaded = 0);

        void wait_interval() const { std::this_thread::sleep_for(this->interval); }

    private:
        void update_interval(const Bencode::BencodeDict& response_dict);

        std::string          announce;
        crypto::Sha1         info_hash;
        std::string          torr_client_id;
        uint16_t             torr_client_port;
        size_t               uploaded{0};
        size_t               downloaded{};
        size_t               torrent_size{};
        bool                 compact{true};
        std::chrono::seconds interval{};
};

}  // namespace torrent
