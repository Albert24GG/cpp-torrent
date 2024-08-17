#pragma once

#include "Crypto.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace torrent::message {

constexpr std::string_view PROTOCOL_IDENTIFIER{"BitTorrent protocol"};
constexpr auto             HANDSHAKE_MESSAGE_SIZE   = 68;
constexpr auto             PROTOCOL_IDENTIFIER_SIZE = 19;
constexpr auto             RESERVED_SIZE            = 8;
constexpr auto             INFO_HASH_SIZE           = crypto::SHA1_SIZE;
constexpr auto             PEER_ID_SIZE             = 20;

using HandshakeMessage = std::array<std::byte, HANDSHAKE_MESSAGE_SIZE>;
using ParsedHandshake  = std::pair<crypto::Sha1, std::array<std::byte, 20>>;

enum MessageType : uint8_t {
    CHOKE = 0,
    UNCHOKE,
    INTERESTED,
    NOT_INTERESTED,
    HAVE,
    BITFIELD,
    REQUEST,
    PIECE,
    CANCEL,
    PORT,
    KEEP_ALIVE,
    NONE,
    INVALID
};

/**
 * @brief Create a handshake message
 *
 * @param info_hash The info hash of the torrent
 * @param peer_id The peer id
 * @return The handshake message
 */
HandshakeMessage create_handshake_message(
    const crypto::Sha1& info_hash, std::span<const char, 20> peer_id
);

/**
 * @brief Parse a handshake message
 *
 * @param handshake_message The handshake message
 * @return A pair consisting of the info hash and the peer id if the handshake message is valid
 */
std::optional<crypto::Sha1> parse_handshake_message(const HandshakeMessage& handshake_message);

};  // namespace torrent::message
