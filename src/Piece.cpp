#include "Piece.hpp"

#include <algorithm>
#include <chrono>
#include <ranges>

namespace torrent {

void Piece::receive_block(std::span<const std::byte> block, size_t offset) {
    auto block_index{get_block_index(offset)};

    // if the block is already received, ignore it
    if (is_block_received(block_index)) {
        return;
    }

    std::ranges::copy(block, std::begin(piece_data_ | std::views::drop(offset)));

    // mark the block as received by moving it to the end of the remaining blocks vector
    auto swapped_block{remaining_blocks_[blocks_left_ - 1]};
    std::swap(
        remaining_blocks_[block_pos_in_rem_[block_index]], remaining_blocks_[blocks_left_ - 1]
    );
    std::swap(block_pos_in_rem_[block_index], block_pos_in_rem_[swapped_block]);

    // decrement the number of blocks left
    --blocks_left_;
}

auto Piece::request_next_block() -> std::optional<std::pair<uint32_t, uint32_t>> {
    for (auto block_index : remaining_blocks_ | std::views::take(blocks_left_)) {
        // if the block has been received or it has not timed out, skip it
        if (is_block_received(block_index) || !is_block_timed_out(block_index)) {
            continue;
        }

        block_request_time_[block_index] = std::chrono::steady_clock::now();
        uint32_t offset{static_cast<uint32_t>(block_index) * BLOCK_SIZE};
        uint32_t block_size{
            block_index == blocks_cnt_ - 1 ? 1 + (piece_size_ - 1) % BLOCK_SIZE : BLOCK_SIZE
        };

        return std::make_pair(offset, block_size);
    }

    return std::nullopt;
}
};  // namespace torrent
