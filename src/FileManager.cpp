#include "FileManager.hpp"

#include "File.hpp"

#include <numeric>
#include <ranges>

namespace torrent::fs {
FileManager::FileManager(
    std::span<const md::FileInfo> files_info, const std::filesystem::path& dest_dir
) {
    for (const auto& file_info : files_info) {
        const std::filesystem::path file_path{dest_dir / file_info.path};

        files_.emplace_back(file_info.start_off, File{file_path, file_info.length});
    }
}

void FileManager::write(std::span<const char> data, size_t offset) {
    size_t file_index{0};

    // find the first file that contains the offset
    while (offset > files_[file_index].first + files_[file_index].second.get_length()) {
        ++file_index;
    }

    // write the data to the file(s)
    while (!data.empty()) {
        auto& [start_off, file] = files_[file_index];

        size_t write_size{start_off + file.get_length() - offset};

        file.write(data | std::views::take(write_size), offset - start_off);

        data = data | std::views::drop(write_size);
        offset += write_size;
        ++file_index;
    }
}

size_t FileManager::get_total_length() const {
    return std::accumulate(
        files_.begin(),
        files_.end(),
        size_t{0},
        [](size_t acc, const auto& file) { return acc + file.second.get_length(); }
    );
}
}  // namespace torrent::fs
