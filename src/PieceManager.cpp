#include "PieceManager.hpp"

#include "Crypto.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <ranges>
#include <span>

namespace torrent {

void PieceManager::update_pieces_availability(const std::vector<bool>& bitfield, int8_t sign) {
    size_t set_bits{0U};
    for (auto piece_idx : std::views::iota(0U, pieces_cnt_)) {
        // Get the bit that represents the availability of the piece
        int8_t cur_piece_avail{static_cast<int8_t>(bitfield[piece_idx])};
        piece_avail_[piece_idx] += sign * cur_piece_avail;
        set_bits += static_cast<size_t>(cur_piece_avail);
    }
    // Require resorting if the number of set bits changed
    are_pieces_sorted_ = (set_bits == 0);
}

void PieceManager::add_available_piece(uint32_t piece_index) {
    // If the pieces are sorted, we can update the availability without resorting the entire vector
    if (are_pieces_sorted_) {
        // The index in the sorted_pieces vector of the last piece with the same availability as the
        // piece to be increased
        auto last_equal_piece =
            std::upper_bound(
                sorted_pieces_.begin(),
                sorted_pieces_.end(),
                piece_avail_[piece_index],
                [&](uint32_t a, uint32_t b) { return piece_avail_[a] < piece_avail_[b]; }
            ) -
            sorted_pieces_.begin() - 1;

        // The index in the sorted_pieces vector of the piece to be increased
        auto increased_piece = last_equal_piece;
        for (; increased_piece > 0 && sorted_pieces_[increased_piece] != piece_index;
             --increased_piece) {
            ;
        }

        // Swap the piece with the last piece that has the same availability
        std::swap(sorted_pieces_[increased_piece], sorted_pieces_[last_equal_piece]);
    }
    ++piece_avail_[piece_index];
}

void PieceManager::receive_block(
    uint32_t piece_index, std::span<const std::byte> block, uint32_t offset
) {
    // If the piece is not requested, ignore the block
    if (!requested_pieces_.contains(piece_index)) {
        return;
    }

    auto& piece = requested_pieces_.at(piece_index);
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
    auto ref_hash{
        crypto::Sha1::from_raw_data(piece_hashes_.data() + piece_index * crypto::SHA1_SIZE)
    };

    if (hash != ref_hash) {
        // If hashes mismatch, mark piecd as incomplete
        piece_completed_[piece_index] = false;
        LOG_WARN("Piece {} hash mismatch. Discarding...", piece_index);
    } else {
        // If hashes match, write the piece to disk
        auto piece_data_char_view{std::span<const char>(
            reinterpret_cast<const char*>(piece_data.data()), piece_data.size()
        )};

        file_manager_->write(piece_data_char_view, piece_index * piece_size_);
        piece_completed_[piece_index] = true;
        if (pieces_left_.fetch_sub(1, std::memory_order_release) == 1) {
            completion_flag_.test_and_set(std::memory_order_release);
        }
    }
    // in either cases, remove the piece from the requested pieces
    requested_pieces_.erase(piece_index);
}

auto PieceManager::request_next_block(const std::vector<bool>& bitfield
) -> std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> {
    if (this->completed()) {
        LOG_DEBUG("No more blocks to download");
        return std::nullopt;
    }

    if (!are_pieces_sorted_) {
        std::sort(sorted_pieces_.begin(), sorted_pieces_.end(), [&](uint32_t a, uint32_t b) {
            return piece_avail_[a] < piece_avail_[b];
        });
        are_pieces_sorted_ = true;
    }

    for (auto piece_idx : sorted_pieces_) {
        // Skip completed pieces or pieces that the peer does not have
        if (piece_completed_[piece_idx] || !bitfield[piece_idx]) {
            continue;
        }

        if (!requested_pieces_.contains(piece_idx)) {
            if (requested_pieces_.size() >= max_active_requests_) {
                continue;
            }
            uint32_t cur_piece_size =
                piece_idx == pieces_cnt_ - 1 ? 1 + (torrent_size_ - 1) % piece_size_ : piece_size_;
            requested_pieces_.emplace(
                piece_idx,
                Piece(cur_piece_size, piece_data_alloc_, piece_util_alloc_, block_request_timeout_)
            );
        }

        auto& piece = requested_pieces_.at(piece_idx);

        auto block_info = piece.request_next_block();

        if (!block_info.has_value()) {
            continue;
        }

        auto [offset, block_size] = *block_info;

        return std::make_tuple(piece_idx, offset, block_size);
    }

    return std::nullopt;
}

}  // namespace torrent
