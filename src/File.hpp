#pragma once

#include <filesystem>

namespace torrent::fs {
class File {
    public:
        File(std::filesystem::path path, size_t start_off, size_t length)
            : path(std::move(path)), start_off(start_off), length(length) {}

        File()                       = default;
        File(const File&)            = default;
        File(File&&)                 = default;
        File& operator=(const File&) = default;
        File& operator=(File&&)      = default;
        ~File()                      = default;

        [[nodiscard]] const std::filesystem::path& get_path() const { return path; }
        [[nodiscard]] size_t                       get_start_off() const { return start_off; }
        [[nodiscard]] size_t                       get_length() const { return length; }

    private:
        std::filesystem::path path;
        size_t                start_off{};
        size_t                length{};
};
}  // namespace torrent::fs
