#pragma once

#include "MemoryPool.hpp"

#include <cstddef>
#include <memory>

namespace torrent::utils {

class PieceAllocator {
    public:
        using value_type = std::byte;

        PieceAllocator(size_t block_size, size_t block_count)
            : pool{std::make_shared<MemoryPool>(block_size, block_count)} {};

        // We will always allocate a blob of memory representing a piece, so we can safely ignore
        // the size
        [[nodiscard]] std::byte* allocate(size_t n) {
            return reinterpret_cast<std::byte*>(pool->allocate());
        }

        void deallocate(std::byte* ptr, size_t n) { pool->deallocate(ptr); }

        template <typename U>
        struct rebind;

    private:
        std::shared_ptr<MemoryPool> pool;
};

template <>
struct PieceAllocator::rebind<std::byte> {
        using other = PieceAllocator;
};

}  // namespace torrent::utils
