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

        /**
         * @brief Write data to the file at a given offset
         *
         * @param data    the data to write
         * @param offset  the offset to write the data at
         * @note The offset is not checked against the length of the file. If the offset is greater
         * than the length of the file, the file will be extended.
         */
        void write(std::span<const char> data, std::size_t offset = 0);

        /**
         * @brief Get the length of the file
         *
         * @return the length of the file
         */
        size_t get_length() const { return length_; }

    private:
        std::fstream file_;
        size_t       length_;
};

}  // namespace torrent::fs
