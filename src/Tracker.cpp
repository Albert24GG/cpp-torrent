#include "Tracker.hpp"

#include "Bencode.hpp"
#include "PeerInfo.hpp"

#include <bit>
#include <cpr/cpr.h>
#include <cstdint>
#include <format>
#include <iostream>
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
        std::ranges::copy(peer | std::views::drop(4), reinterpret_cast<uint8_t*>(&port));

        // if the system is little endian, reverse the order of the bytes
        if constexpr (std::endian::native == std::endian::little) {
            port = std::byteswap(port);
        }

        peer_list.emplace_back(std::format("{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]), port);
    }

    return peer_list;
}

}  // namespace

namespace torrent {

void Tracker::update_interval(const Bencode::BencodeDict& response_dict) {
    this->interval =
        std::chrono::seconds(std::get<Bencode::BencodeInt>(response_dict.at("interval")));
}

// TODO: add logs
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
        return std::nullopt;
    }

    try {
        auto response_dict = std::get<Bencode::BencodeDict>(Bencode::BDecode(response.text));

        this->update_interval(response_dict);

        return extract_peers(response_dict);

    } catch (const std::exception& e) {
        return std::nullopt;
    }
}
}  // namespace torrent
