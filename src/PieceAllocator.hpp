#pragma once

#include "MemoryPool.hpp"

#include <cstddef>

namespace torrent::utils {

class PieceAllocator {
    public:
        using value_type = std::byte;

        PieceAllocator(size_t block_size, size_t block_count) : pool{block_size, block_count} {};

        // We will always allocate a blob of memory representing a piece, so we can safely ignore
        // the size
        [[nodiscard]] std::byte* allocate(size_t n) {
            return reinterpret_cast<std::byte*>(pool.allocate());
        }

        void deallocate(std::byte* ptr, size_t n) { pool.deallocate(ptr); }

        template <typename U>
        struct rebind;

    private:
        MemoryPool pool;
};

template <>
struct PieceAllocator::rebind<std::byte> {
        using other = PieceAllocator;
};

}  // namespace torrent::utils
