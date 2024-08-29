#pragma once

#include "Crypto.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace torrent::message {

inline constexpr std::string_view PROTOCOL_IDENTIFIER{"BitTorrent protocol"};
inline constexpr auto             HANDSHAKE_MESSAGE_SIZE   = 68;
inline constexpr auto             PROTOCOL_IDENTIFIER_SIZE = 19;
inline constexpr auto             RESERVED_SIZE            = 8;
inline constexpr auto             INFO_HASH_SIZE           = crypto::SHA1_SIZE;
inline constexpr auto             PEER_ID_SIZE             = 20;
inline constexpr auto             MAX_SENT_MSG_SIZE        = 17;

using HandshakeMessage = std::array<std::byte, HANDSHAKE_MESSAGE_SIZE>;

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
        std::optional<std::span<std::byte>> payload{std::nullopt};
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
std::optional<crypto::Sha1> parse_handshake_message(
    std::span<const std::byte, HANDSHAKE_MESSAGE_SIZE> handshake_message
);

/**
 * @brief Parse the piece message
 *
 * @param payload The payload of the message
 * @return An optional tuple containing the piece index, the block data and the block offset, or
 *         nullopt if the message is invalid
 */
auto parse_piece_message(std::span<const std::byte> payload
) -> std::optional<std::tuple<uint32_t, std::span<const std::byte>, uint32_t>>;

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
