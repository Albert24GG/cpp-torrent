#pragma once

#include "FileManager.hpp"
#include "Piece.hpp"
#include "PieceAllocator.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace torrent {

static constexpr size_t MAX_MEMPOOL_SIZE{1ULL << 29U};  // 512MB

using namespace std::literals::chrono_literals;
class PieceManager {
    public:
        PieceManager(
            size_t                                    piece_size,
            size_t                                    torrent_size,
            std::shared_ptr<torrent::fs::FileManager> file_manager,
            std::span<const uint8_t>                  piece_hashes,
            std::chrono::milliseconds                 request_timeout = 10s
        )
            : max_active_requests{MAX_MEMPOOL_SIZE / piece_size},
              piece_size{piece_size},
              torrent_size{torrent_size},
              pieces_cnt{1 + (torrent_size - 1) / piece_size},
              block_request_timeout{request_timeout},
              pieces_left{pieces_cnt},
              file_manager{file_manager},
              allocator(piece_size, max_active_requests),
              piece_completed(pieces_cnt, false),
              piece_avail(pieces_cnt, 0),
              piece_hashes{piece_hashes},
              sorted_pieces(PieceComparator(piece_avail)) {
            // TODO: figure out why I cant insert a range
            for (auto i : std::views::iota(0U, pieces_cnt)) {
                sorted_pieces.insert(i);
            }
        }

        void add_peer_bitfield(std::span<const std::byte> bitfield) {
            update_pieces_availability(bitfield, +1);
        }
        void remove_peer_bitfield(std::span<const std::byte> bitfield) {
            update_pieces_availability(bitfield, -1);
        }
        void add_available_piece(size_t piece_index) {
            update_piece_availability(piece_index, piece_avail[piece_index] + 1);
        }

        void receive_block(size_t piece_index, std::span<const std::byte> block, size_t offset);
        auto request_next_block(std::span<const std::byte> bitfield
        ) -> std::optional<std::tuple<uint32_t, size_t, size_t>>;

    private:
        void update_pieces_availability(std::span<const std::byte> bitfield, int8_t sign);

        /**
         * Update the availability of a piece
         * @param piece_index Index of the piece
         * @param availability New availability of the piece
         */
        void update_piece_availability(size_t piece_index, uint16_t availability) {
            sorted_pieces.erase(piece_index);
            piece_avail[piece_index] = availability;
            sorted_pieces.insert(piece_index);
        }

        static uint8_t get_bit(std::span<const std::byte> bitfield, size_t piece_index) {
            return (static_cast<uint8_t>(bitfield[piece_index >> 3]) >> (7 - (piece_index & 7))) &
                   1U;
        }

        size_t                                    max_active_requests;
        size_t                                    piece_size;
        size_t                                    torrent_size;
        size_t                                    pieces_cnt;
        size_t                                    pieces_left;
        std::chrono::milliseconds                 block_request_timeout;
        std::shared_ptr<torrent::fs::FileManager> file_manager;
        torrent::utils::PieceAllocator            allocator;
        std::vector<bool>                         piece_completed;
        // Number of peers that have the ith piece
        std::vector<uint16_t>                      piece_avail;
        std::unordered_map<size_t, torrent::Piece> requested_pieces;
        std::span<const uint8_t>                   piece_hashes;

        class PieceComparator {
            public:
                explicit PieceComparator(const std::vector<uint16_t>& piece_avail)
                    : availability{piece_avail} {}

                bool operator()(uint32_t i, uint32_t j) const {
                    if (availability[i] == availability[j]) {
                        return i < j;
                    }
                    return availability[i] < availability[j];
                }

            private:
                std::span<const uint16_t> availability;
        };

        // Set of indices of pieces sorted by availability
        std::set<uint32_t, PieceComparator> sorted_pieces;
};

}  // namespace torrent
