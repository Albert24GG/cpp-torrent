#pragma once

#include "Crypto.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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

struct Message {
        MessageType                         id{};
        std::optional<std::span<std::byte>> payload = std::nullopt;
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

/**
 * @brief Serialize a message
 *
 * @param msg The message to serialize
 * @param buffer The buffer where the serialized message will be written
 */
void serialize_message(const Message& msg, std::span<std::byte> buffer);

/**
 * @brief Create an interested message
 *
 * @param buffer The buffer where the message will be written
 */
inline void create_interested_message(std::span<std::byte> buffer) {
    serialize_message({MessageType::INTERESTED}, buffer);
}

/**
 * @brief Create a request message
 *
 * @param buffer The buffer where the message will be written
 * @param piece_index The index of the piece
 * @param offset The offset of the block
 * @param length The length of the block
 */
void create_request_message(
    std::span<std::byte> buffer, uint32_t piece_index, uint32_t offset, uint32_t length
);

};  // namespace torrent::message
