#include "MemoryPool.hpp"

#include <cstddef>

namespace torrent::utils {
void* MemoryPool::allocate() {
    if (initialized_blocks < block_count) {
        size_t* ptr{reinterpret_cast<size_t*>(addr_from_index(initialized_blocks))};
        *ptr = initialized_blocks + 1;
        ++initialized_blocks;
    }

    void* return_block{nullptr};

    if (free_blocks > 0) {
        return_block = reinterpret_cast<void*>(next_free_block);
        --free_blocks;
        if (free_blocks > 0) {
            next_free_block = addr_from_index(*reinterpret_cast<size_t*>(next_free_block));
        } else {
            next_free_block = nullptr;
        }
    }
    return return_block;
}
void MemoryPool::deallocate(void* ptr) {
    if (next_free_block != nullptr) {
        *reinterpret_cast<size_t*>(ptr) = index_from_addr(next_free_block);
    } else {
        *reinterpret_cast<size_t*>(ptr) = block_count;
    }
    next_free_block = reinterpret_cast<std::byte*>(ptr);
}

};  // namespace torrent::utils
