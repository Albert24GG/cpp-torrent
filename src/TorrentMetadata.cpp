#include "TorrentMetadata.hpp"

#include "Bencode.hpp"
#include "Error.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <istream>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>

namespace {

inline void check_field_existance(const Bencode::BencodeDict& dict, const std::string& field) {
    if (!dict.contains(field)) {
        err::throw_with_trace(std::format("Invalid torrent file: No \"{}\" field provided.", field)
        );
    }
}

template <typename RequiredType>
inline void check_field_type(const Bencode::BencodeItem& field, const std::string& field_name) {
    if (!std::holds_alternative<RequiredType>(field)) {
        err::throw_with_trace(
            std::format("Invalid torrent file: field \"{}\" holds the wrong type", field_name)
        );
    }
}

template <typename RequiredType>
inline void check_field(const Bencode::BencodeDict& dict, const std::string& field) {
    check_field_existance(dict, field);
    check_field_type<RequiredType>(dict.at(field), field);
}

using Torrent_Info_Content =
    std::tuple<std::string, size_t, std::vector<torrent::md::FileInfo>, std::string>;

using Torrent_Info_File_Entry = std::pair<size_t, std::filesystem::path>;

Torrent_Info_File_Entry parse_file_entry(Bencode::BencodeDict& file_entry) {
    check_field<Bencode::BencodeInt>(file_entry, "length");
    check_field<Bencode::BencodeList>(file_entry, "path");

    auto&                 file_length = std::get<Bencode::BencodeInt>(file_entry["length"]);
    auto&                 path_list   = std::get<Bencode::BencodeList>(file_entry["path"]);
    std::filesystem::path file_path;

    for (auto& path : path_list) {
        file_path = file_path / std::get<Bencode::BencodeString>(path);
    }

    return {file_length, std::move(file_path)};
}

Torrent_Info_Content parse_info(Bencode::BencodeDict& torrent_info) {
    check_field<Bencode::BencodeString>(torrent_info, "name");
    check_field<Bencode::BencodeInt>(torrent_info, "piece length");
    check_field<Bencode::BencodeString>(torrent_info, "pieces");

    if (!torrent_info.contains("length") && !torrent_info.contains("files"))
        err::throw_with_trace(R"(Invalid torrent file: No "length" or "files" field provided.)");

    std::vector<torrent::md::FileInfo> files;

    auto name = std::get<Bencode::BencodeString>(torrent_info["name"]);

    if (torrent_info.contains("length")) {  // single file
        check_field_type<Bencode::BencodeInt>(torrent_info["length"], "length");

        auto& file_length = std::get<Bencode::BencodeInt>(torrent_info["length"]);

        files.emplace_back(name, 0, file_length);
    } else {  // multiple files
        check_field<Bencode::BencodeList>(torrent_info, "files");
        auto   dest_dir  = std::filesystem::path(name);
        auto&  file_list = std::get<Bencode::BencodeList>(torrent_info["files"]);
        size_t cur_offset{0};

        for (auto& file_entry : file_list) {
            check_field_type<Bencode::BencodeDict>(file_entry, "file entry");

            auto [file_length, file_path] =
                parse_file_entry(std::get<Bencode::BencodeDict>(file_entry));

            file_path = dest_dir / file_path;
            files.emplace_back(std::move(file_path), cur_offset, file_length);
            cur_offset += file_length;
        }
    }

    auto piece_length = std::get<Bencode::BencodeInt>(torrent_info["piece length"]);
    auto pieces       = std::move(std::get<Bencode::BencodeString>(torrent_info["pieces"]));
    return {std::move(name), piece_length, std::move(files), std::move(pieces)};
}

torrent::crypto::Sha1 get_info_hash(
    std::istream& torrent_istream, const Bencode::BencodeItem& torrent_info
) {
    size_t start_off{torrent_info.start()};
    size_t length{torrent_info.len()};

    torrent_istream.seekg(start_off, std::ios_base::beg);
    std::vector<uint8_t> buffer(length);

    torrent_istream.read(reinterpret_cast<char*>(buffer.data()), length);

    return torrent::crypto::Sha1::digest(buffer.data(), length);
}

auto parse_announce_list(Bencode::BencodeList& torrent_announce_list
) -> std::vector<std::vector<std::string>> {
    std::vector<std::vector<std::string>> announce_list;

    for (auto& group : torrent_announce_list) {
        check_field_type<Bencode::BencodeList>(group, "announce_group");

        std::vector<std::string> group_announce_list;

        for (auto& announce : std::get<Bencode::BencodeList>(group)) {
            check_field_type<Bencode::BencodeString>(announce, "announce");
            group_announce_list.push_back(std::get<Bencode::BencodeString>(announce));
        }

        announce_list.push_back(std::move(group_announce_list));
    }

    return announce_list;
}

}  // namespace

namespace torrent::md {

TorrentMetadata parse_torrent_file(std::istream& torrent_istream) {
    Bencode::BencodeItem torrent = Bencode::BDecode(torrent_istream);

    if (!std::holds_alternative<Bencode::BencodeDict>(torrent)) {
        err::throw_with_trace("Invalid torrent bencode format.");
    }

    auto& torrent_dict = std::get<Bencode::BencodeDict>(torrent);

    check_field<Bencode::BencodeString>(torrent_dict, "announce");
    check_field<Bencode::BencodeDict>(torrent_dict, "info");

    auto announce_list = [&] -> std::vector<std::vector<std::string>> {
        try {
            check_field<Bencode::BencodeList>(torrent_dict, "announce-list");
            return parse_announce_list(std::get<Bencode::BencodeList>(torrent_dict["announce-list"])
            );
        } catch (const std::exception&) {
            return std::vector<std::vector<std::string>>{};
        }
    }();

    auto& torrent_info = std::get<Bencode::BencodeDict>(torrent_dict["info"]);

    auto [name, piece_length, files, piece_hashes] = parse_info(torrent_info);

    auto info_hash = get_info_hash(torrent_istream, torrent_dict["info"]);

    auto& announce = std::get<Bencode::BencodeString>(torrent_dict["announce"]);

    return {
        .name          = std::move(name),
        .announce      = std::move(announce),
        .announce_list = std::move(announce_list),
        .piece_hashes  = std::move(piece_hashes),
        .piece_length  = piece_length,
        .files         = std::move(files),
        .info_hash     = info_hash
    };
}

TorrentMetadata parse_torrent_file(const std::string& torrent_str) {
    std::istringstream torrent_istream{torrent_str};
    return parse_torrent_file(torrent_istream);
}

}  // namespace torrent::md
