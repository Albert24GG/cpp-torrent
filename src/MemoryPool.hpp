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
            : block_count_{block_count},
              free_blocks_{block_count},
              // Align block size to max align_t
              aligned_block_size_{next_aligned(block_size)},
              pool_(aligned_block_size_ * block_count),
              next_free_block_{pool_.data()} {};

        /**
         * @brief Allocate a block of memory of size n
         *
         * @param size The size of the block to allocate. The size must be less than or equal to the
         * block size of the pool, otherwise the allocation will fail
         * @return A pointer to the allocated memory or nullptr if the allocation failed
         */
        void* allocate(size_t size);

        /**
         * @brief Deallocate a block of memory
         *
         * @param ptr A pointer to the memory block to deallocate, previously allocated with
         * allocate method
         */
        void deallocate(void* ptr);

    private:
        /**
         * @brief Get the address of the block at the given index
         *
         * @param index The index of the block
         * @return The address of the block
         * @note No bounds checking is performed
         */
        std::byte* addr_from_index(size_t index) {
            return pool_.data() + index * aligned_block_size_;
        }

        /**
         * @brief Get the index of the block at the given address
         *
         * @param addr The address of the block
         * @return The index of the block
         * @note No bounds checking is performed
         */
        size_t index_from_addr(std::byte* addr) {
            return (addr - pool_.data()) / aligned_block_size_;
        }

        size_t                 block_count_;
        size_t                 aligned_block_size_;
        size_t                 free_blocks_;
        size_t                 initialized_blocks_{0};
        std::vector<std::byte> pool_;
        std::byte*             next_free_block_{nullptr};
};

}  // namespace torrent::utils
