#include "FileManager.hpp"

#include "File.hpp"

#include <ranges>

namespace torrent::fs {
FileManager::FileManager(
    std::span<const md::FileInfo> files_info, const std::filesystem::path& dest_dir
) {
    for (const auto& file_info : files_info) {
        const std::filesystem::path file_path{dest_dir / file_info.path};

        files.emplace_back(file_info.start_off, File{file_path, file_info.length});
    }
}

void FileManager::write(std::span<const char> data, size_t offset) {
    size_t file_index{0};

    // find the first file that contains the offset
    while (offset < files[file_index].first) {
        ++file_index;
    }

    // write the data to the file(s)
    while (!data.empty()) {
        auto& [start_off, file] = files[file_index];

        size_t write_size{start_off + file.get_length() - offset};

        file.write(data | std::views::take(write_size), offset - start_off);

        data = data | std::views::drop(write_size);
        offset += write_size;
        ++file_index;
    }
}

}  // namespace torrent::fs
