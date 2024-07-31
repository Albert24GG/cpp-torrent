#include "File.hpp"

#include "Error.hpp"

#include <filesystem>
#include <format>

namespace torrent::fs {

File::File(const std::filesystem::path& path, size_t length) : length{length} {
    // create directories if they don't exist
    try {
        std::filesystem::create_directories(path.parent_path());
    } catch (const std::exception& e) {
        err::throw_with_trace(
            std::format("Failed to create directories for file: {} ({})", path.string(), e.what())
        );
    }

    file.open(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        err::throw_with_trace(std::format("Failed to open file: {}", path.string()));
    }
}

File::~File() {
    file.close();
}

void File::write(std::span<const char> data, size_t offset) {
    file.seekp(offset, std::ios::beg);

    if (!file.write(data.data(), data.size())) {
        err::throw_with_trace("Failed to write to file");
    }
}

};  // namespace torrent::fs
