#include "MemoryPool.hpp"

#include <cstddef>
#include <cstring>

namespace torrent::utils {
void* MemoryPool::allocate(size_t size) {
    if (size > aligned_block_size_) {
        return nullptr;
    }

    if (initialized_blocks_ < block_count_) {
        size_t* ptr{reinterpret_cast<size_t*>(addr_from_index(initialized_blocks_))};
        ++initialized_blocks_;
        std::memcpy(ptr, &initialized_blocks_, sizeof(size_t));
    }

    void* return_block{nullptr};

    if (free_blocks_ > 0) {
        return_block = reinterpret_cast<void*>(next_free_block_);
        --free_blocks_;
        if (free_blocks_ > 0) {
            size_t next_index{};
            std::memcpy(&next_index, next_free_block_, sizeof(size_t));
            next_free_block_ = addr_from_index(next_index);
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
        size_t next_index{index_from_addr(next_free_block_)};
        std::memcpy(ptr, &next_index, sizeof(size_t));
    } else {
        std::memcpy(ptr, &block_count_, sizeof(size_t));
    }
    next_free_block_ = reinterpret_cast<std::byte*>(ptr);
    ++free_blocks_;
}

};  // namespace torrent::utils
