#pragma once

#include <filesystem>
#include <fstream>
#include <span>

namespace torrent::fs {

class File {
    public:
        File(const std::filesystem::path& path, size_t length);
        ~File();

        File(const File&)                = delete;
        File& operator=(const File&)     = delete;
        File(File&&) noexcept            = default;
        File& operator=(File&&) noexcept = default;

        void write(std::span<const char> data, std::size_t offset = 0);

        size_t get_length() const { return length; }

    private:
        std::fstream file;
        size_t       length;
};

}  // namespace torrent::fs
