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

static constexpr size_t MAX_MEMPOOL_SIZE{1ULL << 29U};  // 512MB

class PieceManager {
    public:
        PieceManager(
            uint32_t                         piece_size,
            size_t                           torrent_size,
            std::shared_ptr<fs::FileManager> file_manager,
            std::span<const uint8_t>         piece_hashes,
            std::chrono::milliseconds        request_timeout = duration::REQUEST_TIMEOUT
        )
            : max_active_requests{utils::ceil_div(MAX_MEMPOOL_SIZE, piece_size)},
              piece_size{piece_size},
              torrent_size{torrent_size},
              pieces_cnt{utils::ceil_div(torrent_size, piece_size)},
              block_request_timeout{request_timeout},
              pieces_left{pieces_cnt},
              file_manager{std::move(file_manager)},
              piece_data_alloc(piece_size, max_active_requests),
              piece_completed(pieces_cnt, false),
              piece_avail(pieces_cnt),
              piece_hashes{piece_hashes},
              sorted_pieces(pieces_cnt) {
            // Fill the sorted_pieces vector with indices of pieces
            std::iota(sorted_pieces.begin(), sorted_pieces.end(), 0);
        }

        void add_peer_bitfield(const std::vector<bool>& bitfield) {
            update_pieces_availability(bitfield, +1);
        }

        void remove_peer_bitfield(const std::vector<bool>& bitfield) {
            update_pieces_availability(bitfield, -1);
        }

        void add_available_piece(uint32_t piece_index);

        void receive_block(uint32_t piece_index, std::span<const std::byte> block, uint32_t offset);

        auto request_next_block(const std::vector<bool>& bitfield
        ) -> std::optional<std::tuple<uint32_t, uint32_t, uint32_t>>;

        bool completed() const { return pieces_left == 0; }

        size_t get_piece_count() const { return pieces_cnt; }

    private:
        /**
         * Update the availability of pieces
         * @param bitfield Bitfield of the peer
         * @param sign +1 to add, -1 to remove
         */
        void update_pieces_availability(const std::vector<bool>& bitfield, int8_t sign);

        static uint8_t get_bit(std::span<const std::byte> bitfield, uint32_t piece_index) {
            return (static_cast<uint8_t>(bitfield[piece_index >> 3]) >> (7 - (piece_index & 7))) &
                   1U;
        }

        size_t                           max_active_requests;
        uint32_t                         piece_size;
        size_t                           torrent_size;
        size_t                           pieces_cnt;
        size_t                           pieces_left;
        std::chrono::milliseconds        block_request_timeout;
        std::shared_ptr<fs::FileManager> file_manager;
        std::vector<bool>                piece_completed;
        // Number of peers that have the ith piece
        std::vector<uint16_t>               piece_avail;
        std::unordered_map<uint32_t, Piece> requested_pieces;
        std::span<const uint8_t>            piece_hashes;

        // Allocator used for the piece data
        utils::FixedSizeAllocator<std::byte> piece_data_alloc;

        // Vector of indices of pieces sorted by availability
        std::vector<uint32_t> sorted_pieces;
        bool                  are_pieces_sorted{false};
};

}  // namespace torrent
