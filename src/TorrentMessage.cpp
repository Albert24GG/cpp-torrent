#include "TorrentMessage.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <ranges>
#include <span>

namespace torrent::message {

HandshakeMessage create_handshake_message(
    const crypto::Sha1& info_hash, std::span<const char, 20> peer_id
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

std::optional<crypto::Sha1> parse_handshake_message(
    std::span<const std::byte, HANDSHAKE_MESSAGE_SIZE> handshake_message
) {
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

auto parse_piece_message(std::span<const std::byte> payload
) -> std::optional<std::tuple<uint32_t, std::span<const std::byte>, uint32_t>> {
    if (payload.size() < 8) {
        return std::nullopt;
    }

    uint32_t piece_index = [payload] {
        uint32_t index;
        std::ranges::copy(payload | std::views::take(4), reinterpret_cast<std::byte*>(&index));
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(index);
        } else {
            return index;
        }
    }();

    uint32_t offset = [payload] {
        uint32_t off;
        std::ranges::copy(
            payload | std::views::drop(4) | std::views::take(4), reinterpret_cast<std::byte*>(&off)
        );
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(off);
        } else {
            return off;
        }
    }();

    return std::make_tuple(piece_index, payload | std::views::drop(8), offset);
}

void serialize_message(const Message& msg, std::span<std::byte> buffer) {
    assert(buffer.size() > 0 && "Message buffer must be at least 1 byte long");

    if (msg.id == MessageType::KEEP_ALIVE) {
        buffer[0] = static_cast<std::byte>(0);
        return;
    }

    uint32_t message_size =
        1 + msg.payload.transform([](const auto& p) { return p.size(); }).value_or(0);

    assert(buffer.size() >= 4 + message_size && "Message buffer is too small");

    // Copy the message size to the buffer
    {
        uint32_t message_size_network = [message_size] -> uint32_t {
            if constexpr (std::endian::native == std::endian::little) {
                return std::byteswap(message_size);
            } else {
                return message_size;
            }
        }();

        std::ranges::copy(
            std::span<const std::byte, 4>(
                reinterpret_cast<const std::byte*>(&message_size_network), 4
            ),
            std::begin(buffer)
        );
    }

    std::byte id{static_cast<std::byte>(msg.id)};

    // Copy the message id to the buffer
    buffer[4] = id;

    // Copy the payload to the buffer

    if (msg.payload.has_value()) {
        std::ranges::copy(msg.payload.value(), std::begin(buffer | std::views::drop(5)));
    }
}

void create_request_message(
    std::span<std::byte> buffer, uint32_t piece_index, uint32_t offset, uint32_t length
) {
    static std::array<std::byte, 12> request_payload{};

    auto to_network_order_span = [](uint32_t& value) -> std::span<std::byte, 4> {
        value = std::endian::native == std::endian::little ? std::byteswap(value) : value;
        return std::span<std::byte, 4>(reinterpret_cast<std::byte*>(&value), 4);
    };

    // Set the piece index
    std::ranges::copy(to_network_order_span(piece_index), std::begin(request_payload));

    // Set the offset
    std::ranges::copy(
        to_network_order_span(offset), std::begin(request_payload | std::views::drop(4))
    );

    // Set the length
    std::ranges::copy(
        to_network_order_span(length), std::begin(request_payload | std::views::drop(8))
    );

    serialize_message({MessageType::REQUEST, request_payload}, buffer);
}

}  // namespace torrent::message
