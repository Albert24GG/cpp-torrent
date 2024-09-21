#pragma once

#include "Constant.hpp"
#include "Duration.hpp"
#include "FileManager.hpp"
#include "FixedSizeAllocator.hpp"
#include "Piece.hpp"
#include "Utils.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace torrent {

class PieceManager {
    public:
        PieceManager(
            uint32_t                         piece_size,
            size_t                           torrent_size,
            std::shared_ptr<fs::FileManager> file_manager,
            std::span<const uint8_t>         piece_hashes,
            std::chrono::milliseconds        request_timeout = duration::REQUEST_TIMEOUT
        )
            : max_active_requests_{utils::ceil_div(MAX_MEMPOOL_SIZE, piece_size)},
              piece_size_{piece_size},
              torrent_size_{torrent_size},
              pieces_cnt_{utils::ceil_div(torrent_size, piece_size)},
              block_request_timeout_{request_timeout},
              pieces_left_{pieces_cnt_},
              file_manager_{std::move(file_manager)},
              piece_data_alloc_(piece_size, max_active_requests_),
              piece_util_alloc_(
                  utils::ceil_div(piece_size, BLOCK_SIZE) * sizeof(uint16_t),
                  2 * max_active_requests_
              ),
              piece_completed_(pieces_cnt_, false),
              piece_avail_(pieces_cnt_),
              piece_hashes_{piece_hashes},
              sorted_pieces_(pieces_cnt_) {
            // Fill the sorted_pieces vector with indices of pieces
            std::iota(sorted_pieces_.begin(), sorted_pieces_.end(), 0);
        }

        /**
         * @brief Add a peer bitfield
         *
         * @param bitfield Bitfield of the peer
         */
        void add_peer_bitfield(const std::vector<bool>& bitfield) {
            update_pieces_availability(bitfield, +1);
        }

        /**
         * @brief Remove a peer bitfield
         *
         * @param bitfield Bitfield of the peer
         */
        void remove_peer_bitfield(const std::vector<bool>& bitfield) {
            update_pieces_availability(bitfield, -1);
        }

        /**
         * @brief Add a piece to the list of available pieces
         *
         * @param piece_index Index of the piece
         */
        void add_available_piece(uint32_t piece_index);

        /**
         * @brief Receive a requested block
         *
         * @param piece_index Index of the piece
         * @param block Block data
         * @param offset Offset of the block in the piece
         */
        void receive_block(uint32_t piece_index, std::span<const std::byte> block, uint32_t offset);

        /**
         * @brief Request the next block to download
         *
         * @param bitfield Bitfield of the peer
         * @return Index of the piece, offset of the block in the piece, size of the block
         */
        auto request_next_block(const std::vector<bool>& bitfield
        ) -> std::optional<std::tuple<uint32_t, uint32_t, uint32_t>>;

        /**
         * @brief Check if the all the pieces have been downloaded
         *
         * @return True if the torrent is completed
         * @note This function is not thread-safe
         */
        bool completed() const { return pieces_left_ == 0; }

        /**
         * @brief Check if the all the pieces have been downloaded
         *
         * @return True if the torrent is completed
         * @note This function is thread-safe
         */
        bool completed_thread_safe() const {
            return completion_flag_.test(std::memory_order_acquire);
        }

        /**
         * @brief Get the number of pieces in the torrent
         *
         * @return Number of pieces
         */
        size_t get_piece_count() const { return pieces_cnt_; }

        /**
         * @brief Get the number of bytes downloaded
         *
         * @return Number of bytes downloaded
         * @note This function is thread-safe
         */
        size_t get_downloaded_bytes() const {
            return (pieces_cnt_ - pieces_left_.load(std::memory_order_acquire)) * piece_size_;
        }

        /**
         * @brief Check if a block has been received
         *
         * @param piece_index Index of the piece
         * @param block_offset Offset of the block in the piece
         * @return True if the block has been received
         */
        bool is_block_received(uint32_t piece_index, uint32_t block_offset) const {
            assert(piece_index < pieces_cnt_ && "Piece index out of bounds");
            return piece_completed_[piece_index] ||
                   (requested_pieces_.contains(piece_index) &&
                    requested_pieces_.at(piece_index)
                        .is_block_received(Piece::get_block_index(block_offset)));
        }

        /**
         * @brief Get the blocks that have not been received yet
         *        This function is used in the endgame mode
         *
         * @param bitfield Bitfield of the peer
         * @return A vector containing tuples in the form (piece index, block offset, block size)
         */
        auto endgame_remaining_blocks(const std::vector<bool>& bitfield
        ) const -> std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>;

        /**
         * @brief Check if we are in the endgame mode
         *
         * @return True if we are in the endgame mode
         */
        bool is_endgame() const { return endgame_; }

    private:
        /**
         * @brief Update the availability of pieces
         *
         * @param bitfield Bitfield of the peer
         * @param sign +1 to add, -1 to remove
         */
        void update_pieces_availability(const std::vector<bool>& bitfield, int8_t sign);

        size_t                           max_active_requests_;
        uint32_t                         piece_size_;
        size_t                           torrent_size_;
        const size_t                     pieces_cnt_;
        std::atomic<size_t>              pieces_left_;
        std::chrono::milliseconds        block_request_timeout_;
        std::shared_ptr<fs::FileManager> file_manager_;
        std::vector<bool>                piece_completed_;
        // Number of peers that have the ith piece
        std::vector<uint16_t>               piece_avail_;
        std::unordered_map<uint32_t, Piece> requested_pieces_;
        std::span<const uint8_t>            piece_hashes_;

        // Allocator used for the piece data
        utils::FixedSizeAllocator<std::byte> piece_data_alloc_;
        // Allocator used for the vectors that manage remaining blocks
        utils::FixedSizeAllocator<uint16_t> piece_util_alloc_;

        // Vector of indices of pieces sorted by availability
        std::vector<uint32_t> sorted_pieces_;
        bool                  are_pieces_sorted_{false};

        // Atomic flag that indicates completion of the download
        std::atomic_flag completion_flag_{ATOMIC_FLAG_INIT};

        // Flag that indicates if we entered the endgame mode
        bool                                                  endgame_{false};
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> endgame_requests_;
};

}  // namespace torrent
