#pragma once

#include "Duration.hpp"
#include "PieceAllocator.hpp"
#include "Utils.hpp"

#include <chrono>
#include <cstddef>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace torrent {

static constexpr uint32_t BLOCK_SIZE{1ULL << 14U};

class Piece {
    public:
        Piece(
            uint32_t                        size,
            torrent::utils::PieceAllocator& allocator,
            std::chrono::milliseconds       request_timeout = duration::REQUEST_TIMEOUT
        )
            : piece_size{size},
              blocks_cnt{utils::ceil_div(size, BLOCK_SIZE)},
              blocks_left{blocks_cnt},
              block_request_timeout{request_timeout},
              piece_data(size, allocator),
              block_request_time(
                  blocks_cnt, std::chrono::time_point<std::chrono::steady_clock>::min()
              ),
              remaining_blocks(blocks_cnt),
              block_pos_in_rem(blocks_cnt) {
            // Fill the vectors with the indices of the blocks
            for (auto i : std::views::iota(0U, blocks_cnt)) {
                remaining_blocks[i] = block_pos_in_rem[i] = i;
            }
        }
        // block_received(blocks_cnt, false) {}

        /*
         * Receive a previously requested block of data.
         *
         * @param block  the block of data
         * @param offset the offset of the block in the piece
         */
        void receive_block(std::span<const std::byte> block, size_t offset);

        /*
         * Returns the offset of the next block to be requested.
         *
         *@return a pair containing the offset and the size of the block to be requested
                  If there are no more blocks to be requested, returns an empty optional
         */
        auto request_next_block() -> std::optional<std::pair<uint32_t, uint32_t>>;

        [[nodiscard]] bool is_complete() const { return blocks_left == 0; }

        std::span<const std::byte> get_data() { return piece_data; }

    private:
        static uint32_t get_block_index(uint32_t offset) { return offset / BLOCK_SIZE; }

        [[nodiscard]] bool is_block_received(uint16_t block_index) const {
            return block_pos_in_rem[block_index] >= blocks_left;
        }

        [[nodiscard]] bool is_block_timed_out(uint16_t block_index) const {
            return std::chrono::steady_clock::now() - block_request_timeout >
                   block_request_time[block_index];
        }

        uint32_t                                               piece_size;
        size_t                                                 blocks_cnt;
        size_t                                                 blocks_left;
        std::chrono::milliseconds                              block_request_timeout;
        std::vector<std::byte, torrent::utils::PieceAllocator> piece_data;

        // Request time of each block
        // Unrequested = time_point::min()
        std::vector<std::chrono::time_point<std::chrono::steady_clock>> block_request_time;
        // Vector containing indices of blocks that have not been received (will be moving the
        // received blocks to the end, pointed by blocks_left)
        // Using blocks_left as a pointer to the first received block
        std::vector<uint16_t> remaining_blocks;

        // Vector containing the position of each block index in the remaining_blocks vector
        // E.g.: block_pos_in_rem[i] = j -> remaining_block[j] = i
        std::vector<uint16_t> block_pos_in_rem;
};

}  // namespace torrent
