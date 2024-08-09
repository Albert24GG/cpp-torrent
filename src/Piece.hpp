#pragma once

#include "PieceAllocator.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace torrent {

static constexpr size_t BLOCK_SIZE{1ULL << 14U};

using namespace std::literals::chrono_literals;
class Piece {
    public:
        Piece(
            size_t                          size,
            torrent::utils::PieceAllocator& allocator,
            std::chrono::milliseconds       request_timeout = 10s
        )
            : piece_size{size},
              blocks_cnt{1 + (size - 1) / BLOCK_SIZE},
              blocks_left{blocks_cnt},
              block_request_timeout{request_timeout},
              piece_data(size, allocator),
              block_received(blocks_cnt, false) {}

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
        auto request_next_block() -> std::optional<std::pair<size_t, size_t>>;

        bool is_complete() const { return blocks_left == 0; }

        std::span<const std::byte> get_data() { return piece_data; }

    private:
        static size_t get_block_index(size_t offset) { return offset / BLOCK_SIZE; }

        auto get_block_request_time(size_t block_index
        ) const -> std::optional<std::chrono::time_point<std::chrono::steady_clock>> {
            return block_request_time.contains(block_index)
                       ? std::make_optional(block_request_time.at(block_index))
                       : std::nullopt;
        }

        bool is_block_timed_out(size_t block_index) const {
            auto request_time = get_block_request_time(block_index);
            return !request_time.has_value() ||
                   (std::chrono::steady_clock::now() - request_time.value() >= block_request_timeout
                   );
        }

        size_t                                                 piece_size;
        size_t                                                 blocks_cnt;
        size_t                                                 blocks_left;
        std::chrono::milliseconds                              block_request_timeout;
        std::vector<std::byte, torrent::utils::PieceAllocator> piece_data;

        // bitset to keep track of which blocks have been received
        std::vector<bool> block_received;
        // map to keep track of the time when a block was requested
        std::unordered_map<size_t, std::chrono::time_point<std::chrono::steady_clock>>
            block_request_time;
};

}  // namespace torrent
