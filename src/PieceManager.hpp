#pragma once

#include "Duration.hpp"
#include "FileManager.hpp"
#include "FixedSizeAllocator.hpp"
#include "Piece.hpp"
#include "Utils.hpp"

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

inline constexpr size_t MAX_MEMPOOL_SIZE{1ULL << 29U};  // 512MB

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
         */
        bool completed() const { return pieces_left_ == 0; }

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
         */
        size_t get_downloaded_bytes() const { return (pieces_cnt_ - pieces_left_) * piece_size_; }

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
        size_t                           pieces_cnt_;
        size_t                           pieces_left_;
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
};

}  // namespace torrent
