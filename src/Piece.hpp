#pragma once

#include "Duration.hpp"
#include "FixedSizeAllocator.hpp"
#include "Utils.hpp"

#include <chrono>
#include <cstddef>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace torrent {

inline constexpr uint32_t BLOCK_SIZE{1ULL << 14U};

class Piece {
    public:
        Piece(
            uint32_t                                       size,
            torrent::utils::FixedSizeAllocator<std::byte>& piece_data_alloc,
            torrent::utils::FixedSizeAllocator<uint16_t>&  piece_util_alloc,
            std::chrono::milliseconds request_timeout = duration::REQUEST_TIMEOUT
        )
            : piece_size_{size},
              blocks_cnt_{utils::ceil_div(size, BLOCK_SIZE)},
              blocks_left_{blocks_cnt_},
              block_request_timeout_{request_timeout},
              piece_data_(size, piece_data_alloc),
              block_request_time_(
                  blocks_cnt_, std::chrono::time_point<std::chrono::steady_clock>::min()
              ),
              remaining_blocks_(blocks_cnt_, piece_util_alloc),
              block_pos_in_rem_(blocks_cnt_, piece_util_alloc) {
            // Fill the vectors with the indices of the blocks
            for (auto i : std::views::iota(0U, blocks_cnt_)) {
                remaining_blocks_[i] = block_pos_in_rem_[i] = i;
            }
        }

        /**
         * @brief  Receive a previously requested block of data.
         *
         * @param block  the block of data
         * @param offset the offset of the block in the piece
         */
        void receive_block(std::span<const std::byte> block, size_t offset);

        /**
         * @brief Returns the offset of the next block to be requested.
         *
         *@return a pair containing the offset and the size of the block to be requested
                  If there are no more blocks to be requested, returns an empty optional
         */
        auto request_next_block() -> std::optional<std::pair<uint32_t, uint32_t>>;

        /**
         * @brief Check if the piece is complete.
         *
         * @return true if the piece is complete, false otherwise
         */
        [[nodiscard]] bool is_complete() const { return blocks_left_ == 0; }

        /**
         * @brief Get a view to the underlying data of the piece.
         *
         * @return a span containing the data of the piece
         * @note The span is only valid if the piece is complete
         */
        std::span<const std::byte> get_data() { return piece_data_; }

    private:
        /**
         * @brief Get the index of the block containing the given offset.
         *
         * @param offset the offset of the block
         * @return the index of the block
         */
        [[nodiscard]]
        static uint32_t get_block_index(uint32_t offset) {
            return offset / BLOCK_SIZE;
        }

        /**
         * @brief Check if a block has been received.
         *
         * @param block_index the index of the block
         * @return true if the block has been received, false otherwise
         * @note A block is considered received if its position in the remaining_blocks vector is
         * greater than or equal to blocks_left (i.e., it has been moved to the end of the vector)
         */
        [[nodiscard]] bool is_block_received(uint16_t block_index) const {
            return block_pos_in_rem_[block_index] >= blocks_left_;
        }

        /**
         * @brief Check if a block request has timed out.
         *
         * @param block_index the index of the block
         * @return true if the block request has timed out, false otherwise
         */
        [[nodiscard]] bool is_block_timed_out(uint16_t block_index) const {
            return std::chrono::steady_clock::now() - block_request_timeout_ >
                   block_request_time_[block_index];
        }

        uint32_t                  piece_size_;
        size_t                    blocks_cnt_;
        size_t                    blocks_left_;
        std::chrono::milliseconds block_request_timeout_;
        std::vector<std::byte, torrent::utils::FixedSizeAllocator<std::byte>> piece_data_;

        // Request time of each block
        // Unrequested = time_point::min()
        std::vector<std::chrono::time_point<std::chrono::steady_clock>> block_request_time_;
        // Vector containing indices of blocks that have not been received (will be moving the
        // received blocks to the end, pointed by blocks_left)
        // Using blocks_left as a pointer to the first received block
        std::vector<uint16_t, utils::FixedSizeAllocator<uint16_t>> remaining_blocks_;

        // Vector containing the position of each block index in the remaining_blocks vector
        // E.g.: block_pos_in_rem[i] = j -> remaining_block[j] = i
        std::vector<uint16_t, utils::FixedSizeAllocator<uint16_t>> block_pos_in_rem_;
};

}  // namespace torrent
