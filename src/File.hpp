#pragma once

#include <filesystem>

namespace torrent::fs {
class File {
    public:
        File(std::filesystem::path path, size_t offset_start, size_t length)
            : path(std::move(path)), offset_start(offset_start), length(length) {}

        File()                       = default;
        File(const File&)            = default;
        File(File&&)                 = default;
        File& operator=(const File&) = default;
        File& operator=(File&&)      = default;
        ~File()                      = default;

    private:
        std::filesystem::path path;
        size_t                offset_start{};
        size_t                length{};
};
}  // namespace torrent::fs
