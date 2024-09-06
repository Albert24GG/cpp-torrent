#include "MemoryPool.hpp"

#include <cstddef>

namespace torrent::utils {
void* MemoryPool::allocate(size_t size) {
    if (size > aligned_block_size_) {
        return nullptr;
    }

    if (initialized_blocks_ < block_count_) {
        size_t* ptr{reinterpret_cast<size_t*>(addr_from_index(initialized_blocks_))};
        *ptr = initialized_blocks_ + 1;
        ++initialized_blocks_;
    }

    void* return_block{nullptr};

    if (free_blocks_ > 0) {
        return_block = reinterpret_cast<void*>(next_free_block_);
        --free_blocks_;
        if (free_blocks_ > 0) {
            next_free_block_ = addr_from_index(*reinterpret_cast<size_t*>(next_free_block_));
        } else {
            next_free_block_ = nullptr;
        }
    }
    return return_block;
}
void MemoryPool::deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    if (next_free_block_ != nullptr) {
        *reinterpret_cast<size_t*>(ptr) = index_from_addr(next_free_block_);
    } else {
        *reinterpret_cast<size_t*>(ptr) = block_count_;
    }
    next_free_block_ = reinterpret_cast<std::byte*>(ptr);
    ++free_blocks_;
}

};  // namespace torrent::utils
