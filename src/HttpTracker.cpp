#include "HttpTracker.hpp"

#include "Bencode.hpp"
#include "Logger.hpp"
#include "PeerInfo.hpp"
#include "Utils.hpp"

#include <bit>
#include <cpr/cpr.h>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <variant>
#include <vector>

namespace {

std::optional<std::vector<torrent::PeerInfo>> extract_peers(
    const Bencode::BencodeDict& response_dict
) {
    auto& peers = std::get<Bencode::BencodeString>(response_dict.at("peers"));

    // number of bytes in the peerlist should be a multiple of 6
    if (peers.size() % 6 != 0) {
        return std::nullopt;
    }

    std::vector<torrent::PeerInfo> peer_list;
    peer_list.reserve(peers.size() / 6);

    for (auto peer : peers | std::views::chunk(6)) {
        std::array<uint8_t, 4> ip{};
        uint16_t               port{};

        std::ranges::copy(peer | std::views::take(4), std::ranges::begin(ip));
        std::ranges::copy(
            peer | std::views::drop(4) | std::views::take(2), reinterpret_cast<uint8_t*>(&port)
        );

        // if the system is little endian, reverse the order of the bytes
        port = torrent::utils::host_to_network_order(port);

        peer_list.emplace_back(std::format("{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]), port);
    }

    LOG_INFO("Extracted {} peers from tracker response", peer_list.size());
    return peer_list;
}

}  // namespace

namespace torrent {

void HttpTracker::update_interval(const Bencode::BencodeDict& response_dict) {
    interval_ = std::chrono::seconds(std::get<Bencode::BencodeInt>(response_dict.at("interval")));
}

std::optional<std::vector<PeerInfo>> HttpTracker::retrieve_peers(
    size_t downloaded, size_t uploaded
) {
    // convert the info hash to a string
    std::string info_hash_str{info_hash_.get().begin(), info_hash_.get().end()};

    cpr::Response response = cpr::Get(
        cpr::Url{announce_},
        cpr::Parameters{
            {"info_hash", info_hash_str},
            {"peer_id", torr_client_id_},
            {"port", std::to_string(torr_client_port_)},
            {"uploaded", std::to_string(uploaded)},
            {"downloaded", std::to_string(downloaded)},
            {"left", std::to_string(torrent_size_ - downloaded)},
            {"compact", compact_ ? "1" : "0"}
        }
    );

    // if the request failed, return an empty optional
    if (response.status_code != 200) {
        LOG_ERROR(
            "Failed to retrieve peers from tracker with status code: {}", response.status_code
        );
        return std::nullopt;
    }

    LOG_INFO("Successfully retrieved peers from tracker");

    try {
        auto response_dict = std::get<Bencode::BencodeDict>(Bencode::BDecode(response.text));

        update_interval(response_dict);
        LOG_DEBUG("Tracker interval: {}s", interval_.count());

        return extract_peers(response_dict);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse tracker response: {}", e.what());
        return std::nullopt;
    }
}
}  // namespace torrent
