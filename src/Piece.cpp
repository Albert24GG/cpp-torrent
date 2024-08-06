#include "Piece.hpp"

#include <chrono>
#include <ranges>

namespace torrent {

void Piece::receive_block(std::span<const std::byte> block, size_t offset) {
    size_t block_index{offset / BLOCK_SIZE};

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

auto Piece::request_next_block() -> std::optional<std::pair<size_t, size_t>> {
    if (blocks_left == 0) {
        return std::nullopt;
    }

    for (auto i : std::views::iota(0ULL, blocks_cnt)) {
        // if the block has been received and it has not timed out, skip it
        if (block_received[i] || !is_block_timed_out(i)) {
            continue;
        }
        block_request_time.emplace(i, std::chrono::steady_clock::now());
        size_t offset{i * BLOCK_SIZE};
        size_t block_size{i == blocks_cnt - 1 ? piece_size % BLOCK_SIZE : BLOCK_SIZE};

        return std::make_pair(offset, block_size);
    }

    return std::nullopt;
}
};  // namespace torrent
