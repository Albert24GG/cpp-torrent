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
        FileManager(FileManager&&)                 = default;
        FileManager& operator=(FileManager&&)      = default;
        ~FileManager()                             = default;

        /**
         * @brief Write data at a given offset
         * The correct file is found by checking the offset against the start_offset and length of
         * each file
         *
         * @param data    the data to write
         * @param offset  the offset to write the data at
         */
        void write(std::span<const char> data, size_t offset);

        /**
         * @brief Get the total length of all the files
         *
         * @return the total length of all the files
         */
        [[nodiscard]] size_t get_total_length() const;

    private:
        // pair containing the file start_offset and the file object
        std::vector<std::pair<size_t, File>> files_;
};

}  // namespace torrent::fs
