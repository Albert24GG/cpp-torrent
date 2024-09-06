#pragma once

#include "MemoryPool.hpp"

#include <cstddef>
#include <memory>

namespace torrent::utils {

template <typename T>
class FixedSizeAllocator {
    public:
        using value_type = T;

        FixedSizeAllocator(size_t block_size, size_t block_count)
            : pool_{std::make_shared<MemoryPool>(block_size, block_count)} {};

        /** @brief Allocate a block of memory of size n * sizeof(T)
         *
         * @param n The number of elements to allocate
         * The size if only used for confirmation purposes. If the size if greater than the block
         * size, the allocation will fail and return nullptr
         * @return A pointer to the allocated memory or nullptr if the allocation failed
         */
        [[nodiscard]] T* allocate(size_t n) {
            return reinterpret_cast<T*>(pool_->allocate(n * sizeof(T)));
        }

        /** @brief Deallocate a block of memory
         *
         * @param ptr A pointer to the memory block to deallocate, previously allocated with
         * allocate method
         * @param n The number of elements in the block. This parameter is ignored
         */
        void deallocate(T* ptr, size_t n) { pool_->deallocate(ptr); }

        template <typename U>
        struct rebind {
                using other = FixedSizeAllocator<U>;
        };

    private:
        std::shared_ptr<MemoryPool> pool_;
};

}  // namespace torrent::utils
