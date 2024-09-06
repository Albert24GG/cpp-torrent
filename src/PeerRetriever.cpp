#include "PeerRetriever.hpp"

#include "HttpTracker.hpp"
#include "UdpTracker.hpp"

#include <string_view>

namespace {

enum class TrackerType { HTTP, UDP, UNKNOWN };

TrackerType get_tracker_type(std::string_view announce) {
    if (announce.starts_with("http")) {
        return TrackerType::HTTP;
    } else if (announce.starts_with("udp")) {
        return TrackerType::UDP;
    } else {
        return TrackerType::UNKNOWN;
    }
}
}  // namespace

namespace torrent {

auto PeerRetriever::retrieve_peers(size_t downloaded, size_t uploaded)
    -> std::optional<std::vector<PeerInfo>> {
    // Try to retrieve peers from the current tracker
    auto peer_list =
        cur_tracker_ != nullptr ? cur_tracker_->retrieve_peers(downloaded, uploaded) : std::nullopt;

    if (peer_list.has_value()) {
        return peer_list;
    }

    for (auto& announce_group : announce_list_) {
        for (auto& announce : announce_group) {
            switch (get_tracker_type(announce)) {
                case TrackerType::HTTP:
                    cur_tracker_ = std::make_unique<HttpTracker>(
                        announce, info_hash_, client_id_, client_port_, torrent_size_
                    );
                    break;

                case TrackerType::UDP:
                    cur_tracker_ = std::make_unique<UdpTracker>(
                        announce, info_hash_, client_id_, client_port_, torrent_size_
                    );
                    break;

                case TrackerType::UNKNOWN:
                    continue;
            }

            peer_list = cur_tracker_->retrieve_peers(downloaded, uploaded);

            // If we successfully retrieved peers, move the current announce to the front of the
            // group
            if (peer_list.has_value()) {
                std::swap(announce_group.front(), announce);
                return peer_list;
            }
        }
    }

    return peer_list;
}

}  // namespace torrent
