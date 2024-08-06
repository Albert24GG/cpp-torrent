#include "PieceManager.hpp"

#include "Crypto.hpp"

#include <cstdint>
#include <ranges>
#include <span>

namespace torrent {

void PieceManager::update_pieces_availability(std::span<const std::byte> bitfield, char sign) {
    for (auto piece_idx : std::views::iota(0U, pieces_cnt)) {
        // Get the bit that represents the availability of the piece
        uint8_t avail{get_bit(bitfield, piece_idx)};

        piece_avail[piece_idx] += sign * avail;
        if (avail != 0U) {
            sorted_pieces.insert(piece_idx);
        }
    }
}

void PieceManager::receive_block(
    size_t piece_index, std::span<const std::byte> block, size_t offset
) {
    // If the piece is not requested, ignore the block
    if (!requested_pieces.contains(piece_index)) {
        return;
    }

    auto& piece = requested_pieces.at(piece_index);
    piece.receive_block(block, offset);

    if (!piece.is_complete()) {
        return;
    }

    // Piece is complete

    auto piece_data{piece.get_data()};
    auto piece_data_uint8_view{std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(piece_data.data()), piece_data.size()
    )};

    auto hash{crypto::Sha1::digest(piece_data_uint8_view)};
    auto ref_hash{crypto::Sha1::from_raw_data(piece_hashes.data() + piece_index * crypto::SHA1_SIZE)
    };

    if (hash != ref_hash) {
        // If hashes mismatch, mark piecd as incomplete
        piece_completed[piece_index] = false;
    } else {
        // If hashes match, write the piece to disk
        auto piece_data_char_view{std::span<const char>(
            reinterpret_cast<const char*>(piece_data.data()), piece_data.size()
        )};

        file_manager->write(piece_data_char_view, piece_index * piece_size);
        piece_completed[piece_index] = true;
        --pieces_left;
    }
    // in either cases, remove the piece from the requested pieces
    requested_pieces.erase(piece_index);
}

auto PieceManager::request_next_block(std::span<const std::byte> bitfield
) -> std::optional<std::tuple<uint32_t, size_t, size_t>> {
    if (pieces_left == 0) {
        return std::nullopt;
    }

    for (auto piece_idx : sorted_pieces) {
        // Skip completed pieces or pieces that the peer does not have
        if (piece_completed[piece_idx] || get_bit(bitfield, piece_idx) == 0) {
            continue;
        }

        if (!requested_pieces.contains(piece_idx)) {
            if (requested_pieces.size() >= max_active_requests) {
                continue;
            }
            requested_pieces.emplace(piece_idx, Piece(piece_size, allocator));
        }

        auto& piece = requested_pieces.at(piece_idx);

        auto block_info = piece.request_next_block();

        if (!block_info.has_value()) {
            continue;
        }

        auto [offset, block_size] = block_info.value();

        return std::make_tuple(piece_idx, offset, block_size);
    }

    return std::nullopt;
}

}  // namespace torrent
