#include "PieceManager.hpp"

#include "Crypto.hpp"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <span>
#include <spdlog/spdlog.h>

namespace torrent {

void PieceManager::update_pieces_availability(std::span<const std::byte> bitfield, int8_t sign) {
    for (auto piece_idx : std::views::iota(0U, pieces_cnt)) {
        // Get the bit that represents the availability of the piece
        uint8_t avail{get_bit(bitfield, piece_idx)};
        piece_avail[piece_idx] += sign * avail;
    }
    are_pieces_sorted = false;
}

void PieceManager::add_available_piece(uint32_t piece_index) {
    // If the pieces are sorted, we can update the availability without resorting the entire vector
    if (are_pieces_sorted) {
        // The index in the sorted_pieces vector of the last piece with the same availability as the
        // piece to be increased
        auto last_equal_piece =
            std::upper_bound(
                sorted_pieces.begin(),
                sorted_pieces.end(),
                piece_avail[piece_index],
                [&](uint32_t a, uint32_t b) { return piece_avail[a] < piece_avail[b]; }
            ) -
            sorted_pieces.begin() - 1;

        // The index in the sorted_pieces vector of the piece to be increased
        auto increased_piece = last_equal_piece;
        for (; increased_piece > 0 && sorted_pieces[increased_piece] != piece_index;
             --increased_piece) {
            ;
        }

        // Swap the piece with the last piece that has the same availability
        std::swap(sorted_pieces[increased_piece], sorted_pieces[last_equal_piece]);
    }
    ++piece_avail[piece_index];
}

void PieceManager::receive_block(
    uint32_t piece_index, std::span<const std::byte> block, uint32_t offset
) {
    // If the piece is not requested, ignore the block
    if (!requested_pieces.contains(piece_index)) {
        return;
    }

    auto& piece = requested_pieces.at(piece_index);
    piece.receive_block(block, offset);

    spdlog::debug("Received block at offset {} for piece {}", offset, piece_index);

    if (!piece.is_complete()) {
        return;
    }

    // Piece is complete
    spdlog::debug("Piece {} is complete", piece_index);

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
        spdlog::warn("Piece {} hash mismatch. Discarding...", piece_index);
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
) -> std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> {
    if (pieces_left == 0) {
        spdlog::debug("No more blocks to download");
        return std::nullopt;
    }

    if (!are_pieces_sorted) {
        std::sort(sorted_pieces.begin(), sorted_pieces.end(), [&](uint32_t a, uint32_t b) {
            return piece_avail[a] < piece_avail[b];
        });
        are_pieces_sorted = true;
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
            uint32_t cur_piece_size =
                piece_idx == pieces_cnt - 1 ? 1 + (torrent_size - 1) % piece_size : piece_size;
            requested_pieces.emplace(
                piece_idx, Piece(cur_piece_size, allocator, block_request_timeout)
            );
        }

        auto& piece = requested_pieces.at(piece_idx);

        auto block_info = piece.request_next_block();

        if (!block_info.has_value()) {
            continue;
        }

        auto [offset, block_size] = block_info.value();

        spdlog::debug("Requesting block at offset {} for piece {}", offset, piece_idx);

        return std::make_tuple(piece_idx, offset, block_size);
    }

    spdlog::debug("No available block to request for specified bitfield");

    return std::nullopt;
}

}  // namespace torrent
