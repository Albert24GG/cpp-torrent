#pragma once

#include "File.hpp"
#include "TorrentMetadata.hpp"

#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace torrent::fs {

class FileManager {
    public:
        explicit FileManager(
            std::span<const md::FileInfo> files_info, const std::filesystem::path& dest_dir = "."
        );

        FileManager(const FileManager&)            = delete;
        FileManager& operator=(const FileManager&) = delete;
        FileManager(FileManager&&)                 = delete;
        FileManager& operator=(FileManager&&)      = delete;
        ~FileManager()                             = default;

        void write(std::span<const char> data, size_t offset);

    private:
        // pair containing the file start_offset and the file object
        std::vector<std::pair<size_t, File>> files;
};

}  // namespace torrent::fs
