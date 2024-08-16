#include "TorrentMessage.hpp"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <span>

namespace torrent::message {

HandshakeMessage create_handshake_message(
    const crypto::Sha1& info_hash, std::span<char, 20> peer_id
) {
    HandshakeMessage handshake_message{};

    // Set the protocol identifier length
    handshake_message[0] = static_cast<std::byte>(PROTOCOL_IDENTIFIER_SIZE);

    // Set the protocol identifier
    std::ranges::copy(
        std::as_bytes(std::span<const char>(PROTOCOL_IDENTIFIER)),
        std::begin(
            handshake_message | std::views::drop(1) | std::views::take(PROTOCOL_IDENTIFIER_SIZE)
        )
    );

    // Set the reserved bytes
    std::ranges::fill(
        handshake_message | std::views::drop(1 + PROTOCOL_IDENTIFIER_SIZE) |
            std::views::take(RESERVED_SIZE),
        std::byte{0}
    );

    // Set the info hash
    std::ranges::copy(
        std::as_bytes(info_hash.get()),
        std::begin(
            handshake_message | std::views::drop(1 + PROTOCOL_IDENTIFIER_SIZE + RESERVED_SIZE) |
            std::views::take(INFO_HASH_SIZE)
        )
    );

    // Set the peer id
    std::ranges::copy(
        std::as_bytes(peer_id),
        std::begin(
            handshake_message |
            std::views::drop(1 + PROTOCOL_IDENTIFIER_SIZE + RESERVED_SIZE + INFO_HASH_SIZE) |
            std::views::take(PEER_ID_SIZE)
        )
    );

    return handshake_message;
}

std::optional<crypto::Sha1> parse_handshake_message(const HandshakeMessage& handshake_message) {
    // The message length is already checked by the caller to be 68

    // Check the protocol identifier length
    if (handshake_message[0] != static_cast<std::byte>(PROTOCOL_IDENTIFIER_SIZE)) {
        return std::nullopt;
    }

    // Check the protocol identifier
    if (!std::ranges::equal(
            handshake_message | std::views::drop(1) | std::views::take(PROTOCOL_IDENTIFIER_SIZE),
            std::as_bytes(std::span<const char>(PROTOCOL_IDENTIFIER))
        )) {
        return std::nullopt;
    }

    // Skip the reserved bytes

    // Extract the info hash
    auto info_hash_view{
        handshake_message | std::views::drop(1 + PROTOCOL_IDENTIFIER_SIZE + RESERVED_SIZE) |
        std::views::take(INFO_HASH_SIZE)
    };
    crypto::Sha1 info_hash{crypto::Sha1::from_raw_data(std::span<const uint8_t, INFO_HASH_SIZE>(
        reinterpret_cast<const uint8_t*>(info_hash_view.data()), INFO_HASH_SIZE
    ))};

    return info_hash;
}

};  // namespace torrent::message
