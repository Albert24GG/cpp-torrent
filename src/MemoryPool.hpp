#pragma once

#include "Utils.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

// Simple Segregated Storage based memory pool
// https://arxiv.org/pdf/2210.16471

// TODO: implement resizing
namespace torrent::utils {

class MemoryPool {
    public:
        MemoryPool(size_t block_size, size_t block_count)
            : block_count{block_count},
              free_blocks{block_count},
              // Align block size to max align_t
              aligned_block_size{next_aligned(block_size)},
              pool(aligned_block_size * block_count),
              next_free_block{pool.data()} {};

        /**
         * Allocate a block of memory of size n
         *
         * @param size The size of the block to allocate. The size must be less than or equal to the
         * block size of the pool, otherwise the allocation will fail
         * @return A pointer to the allocated memory or nullptr if the allocation failed
         */
        void* allocate(size_t size);

        /**
         * Deallocate a block of memory
         *
         * @param ptr A pointer to the memory block to deallocate, previously allocated with
         * allocate method
         */
        void deallocate(void* ptr);

    private:
        std::byte* addr_from_index(size_t index) {
            return pool.data() + index * aligned_block_size;
        }

        size_t index_from_addr(std::byte* addr) {
            return (addr - pool.data()) / aligned_block_size;
        }

        size_t                 block_count;
        size_t                 aligned_block_size;
        size_t                 free_blocks;
        size_t                 initialized_blocks{0};
        std::vector<std::byte> pool;
        std::byte*             next_free_block{nullptr};
};

}  // namespace torrent::utils
