#include "Tracker.hpp"

#include "Bencode.hpp"
#include "PeerInfo.hpp"
#include "Utils.hpp"

#include <bit>
#include <cpr/cpr.h>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#include <variant>
#include <vector>

namespace {

std::optional<std::vector<torrent::PeerInfo>> extract_peers(
    const Bencode::BencodeDict& response_dict
) {
    auto& peers = std::get<Bencode::BencodeString>(response_dict.at("peers"));

    spdlog::debug("Peers: {}", spdlog::to_hex(peers));

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
        std::ranges::copy(peer | std::views::drop(4), reinterpret_cast<uint8_t*>(&port));

        // if the system is little endian, reverse the order of the bytes
        port = torrent::utils::host_to_network_order(port);

        peer_list.emplace_back(std::format("{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]), port);
    }

    spdlog::info("Extracted {} peers from tracker response", peer_list.size());
    return peer_list;
}

}  // namespace

namespace torrent {

void Tracker::update_interval(const Bencode::BencodeDict& response_dict) {
    this->interval =
        std::chrono::seconds(std::get<Bencode::BencodeInt>(response_dict.at("interval")));
}

std::optional<std::vector<PeerInfo>> Tracker::retrieve_peers(size_t downloaded, size_t uploaded) {
    // convert the info hash to a string
    std::string info_hash_str{this->info_hash.get().begin(), this->info_hash.get().end()};

    cpr::Response response = cpr::Get(
        cpr::Url{this->announce},
        cpr::Parameters{
            {"info_hash", info_hash_str},
            {"peer_id", this->torr_client_id},
            {"port", std::to_string(this->torr_client_port)},
            {"uploaded", std::to_string(uploaded)},
            {"downloaded", std::to_string(downloaded)},
            {"left", std::to_string(this->torrent_size - downloaded)},
            {"compact", this->compact ? "1" : "0"}
        }
    );

    // if the request failed, return an empty optional
    if (response.status_code != 200) {
        spdlog::error(
            "Failed to retrieve peers from tracker with status code: {}", response.status_code
        );
        return std::nullopt;
    }

    spdlog::info("Successfully retrieved peers from tracker");

    try {
        auto response_dict = std::get<Bencode::BencodeDict>(Bencode::BDecode(response.text));

        this->update_interval(response_dict);
        spdlog::debug("Tracker interval: {}s", this->interval.count());

        return extract_peers(response_dict);

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse tracker response: {}", e.what());
        return std::nullopt;
    }
}
}  // namespace torrent
