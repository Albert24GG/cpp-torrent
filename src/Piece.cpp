#include "Piece.hpp"

#include <chrono>
#include <ranges>

namespace torrent {

void Piece::receive_block(std::span<const std::byte> block, size_t offset) {
    auto block_index{get_block_index(offset)};

    // if the block is already complete, ignore it
    if (block_received[block_index]) {
        return;
    }

    std::ranges::copy(block, std::begin(piece_data | std::views::drop(offset)));

    // mark the block as received
    block_received[block_index] = true;

    // remove the block from the request time map
    block_request_time.erase(block_index);

    // decrement the number of blocks left
    --blocks_left;
}

auto Piece::request_next_block() -> std::optional<std::pair<uint32_t, uint32_t>> {
    if (blocks_left == 0) {
        return std::nullopt;
    }

    for (auto i : std::views::iota(0U, blocks_cnt)) {
        // if the block has been received and it has not timed out, skip it
        if (block_received[i] || !is_block_timed_out(i)) {
            continue;
        }
        block_request_time.insert_or_assign(i, std::chrono::steady_clock::now());
        uint32_t offset{i * BLOCK_SIZE};
        uint32_t block_size{i == blocks_cnt - 1 ? 1 + (piece_size - 1) % BLOCK_SIZE : BLOCK_SIZE};

        return std::make_pair(offset, block_size);
    }

    return std::nullopt;
}
};  // namespace torrent
