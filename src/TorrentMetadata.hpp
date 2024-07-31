#pragma once

#include "Crypto.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace torrent::md {

struct FileInfo {
        std::filesystem::path path;
        size_t                start_off{};
        size_t                length{};
};

struct TorrentMetadata {
        std::string           announce;
        std::string           piece_hashes;
        size_t                piece_length;
        std::vector<FileInfo> files;
        crypto::Sha1          info_hash;
};

TorrentMetadata parse_torrent_file(std::istream& torrent_istream);
TorrentMetadata parse_torrent_file(const std::string& torrent_str);

}  // namespace torrent::md
