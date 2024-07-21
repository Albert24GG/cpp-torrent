#pragma once

#include "Crypto.hpp"
#include "File.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace torrent::md {

struct TorrentFile {
        std::string           announce;
        std::string           piece_hashes;
        size_t                piece_length;
        std::vector<fs::File> files;
        crypto::Sha1          info_hash;
};

TorrentFile parse_torrent_file(std::istream& torrent_istream);
TorrentFile parse_torrent_file(const std::filesystem::path& torrent_path);
TorrentFile parse_torrent_file(const std::string& torrent_str);

}  // namespace torrent::md
