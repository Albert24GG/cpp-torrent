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
        std::string                           announce;
        std::vector<std::vector<std::string>> announce_list;
        std::string                           piece_hashes;
        size_t                                piece_length;
        std::vector<FileInfo>                 files;
        crypto::Sha1                          info_hash;
};

/**
 * @brief Parse the given torrent file
 *
 * @param torrent_istream  the input stream of the torrent file
 * @return the parsed torrent metadata
 */
TorrentMetadata parse_torrent_file(std::istream& torrent_istream);

/**
 * @brief Parse the given torrent file
 *
 * @param torrent_str  the string representation of the torrent file
 * @return the parsed torrent metadata
 */
TorrentMetadata parse_torrent_file(const std::string& torrent_str);

}  // namespace torrent::md
