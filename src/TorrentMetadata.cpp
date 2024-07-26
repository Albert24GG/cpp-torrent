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
    if (!dict.contains(field))
        err::throw_with_trace(std::format("Invalid torrent file: No \"{}\" field provided.", field)
        );
}

template <typename RequiredType, typename FieldType>
inline void check_field_type(FieldType&& field, const std::string& field_name) {
    if (std::is_same_v<RequiredType, std::decay_t<FieldType>>)
        err::throw_with_trace(
            std::format("Invalid torrent file: field \"{}\" holds the wrong type", field_name)
        );
}

template <typename RequiredType>
inline void check_field(const Bencode::BencodeDict& dict, const std::string& field) {
    check_field_existance(dict, field);
    check_field_type<RequiredType>(dict.at(field), field);
}

using Torrent_Info_Content = std::tuple<size_t, std::vector<torrent::fs::File>, std::string>;

using Torrent_Info_File_Entry = std::pair<size_t, std::filesystem::path>;

Torrent_Info_File_Entry parse_file_entry(Bencode::BencodeDict& file_entry) {
    check_field<Bencode::BencodeInt>(file_entry, "length");
    check_field<Bencode::BencodeList>(file_entry, "path");

    auto&                 file_length = std::get<Bencode::BencodeInt>(file_entry["length"]);
    auto&                 path_list   = std::get<Bencode::BencodeList>(file_entry["path"]);
    std::filesystem::path file_path;

    for (auto& path : path_list) {
        check_field_type<Bencode::BencodeString>(path, "path");
        file_path = file_path / std::move(std::get<Bencode::BencodeString>(path));
    }

    return {file_length, std::move(file_path)};
}

Torrent_Info_Content parse_info(Bencode::BencodeDict& torrent_info) {
    std::filesystem::path dest_dir{std::filesystem::current_path()};

    check_field<Bencode::BencodeString>(torrent_info, "name");
    check_field<Bencode::BencodeInt>(torrent_info, "piece length");
    check_field<Bencode::BencodeString>(torrent_info, "pieces");

    if (!torrent_info.contains("length") && !torrent_info.contains("files"))
        err::throw_with_trace(R"(Invalid torrent file: No "length" or "files" field provided.)");

    std::vector<torrent::fs::File> files;

    if (torrent_info.contains("length")) {  // single file
        check_field_type<Bencode::BencodeInt>(torrent_info["length"], "length");

        auto& file_length = std::get<Bencode::BencodeInt>(torrent_info["length"]);
        auto  file_path =
            dest_dir / std::move(std::get<Bencode::BencodeString>(torrent_info["name"]));

        files.emplace_back(std::move(file_path), 0, file_length);
    } else {  // multiple files
        check_field<Bencode::BencodeList>(torrent_info, "files");
        dest_dir = dest_dir / std::move(std::get<Bencode::BencodeString>(torrent_info["name"]));
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
    return {piece_length, std::move(files), std::move(pieces)};
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

}  // namespace

namespace torrent::md {

TorrentMetadata parse_torrent_file(std::istream& torrent_istream) {
    Bencode::BencodeItem torrent = Bencode::BDecode(torrent_istream);

    if (!std::holds_alternative<Bencode::BencodeDict>(torrent))
        err::throw_with_trace("Invalid torrent bencode format.");

    auto& torrent_dict = std::get<Bencode::BencodeDict>(torrent);

    check_field<Bencode::BencodeString>(torrent_dict, "announce");
    check_field<Bencode::BencodeString>(torrent_dict, "info");

    auto& torrent_info = std::get<Bencode::BencodeDict>(torrent_dict["info"]);

    auto [piece_length, files, piece_hashes] = parse_info(torrent_info);

    auto info_hash = get_info_hash(torrent_istream, torrent_dict["info"]);

    auto& announce = std::get<Bencode::BencodeString>(torrent_dict["announce"]);

    return {
        .announce     = std::move(announce),
        .piece_hashes = std::move(piece_hashes),
        .piece_length = piece_length,
        .files        = std::move(files),
        .info_hash    = info_hash
    };
}

TorrentMetadata parse_torrent_file(const std::filesystem::path& torrent_path) {
    if (!std::filesystem::exists(torrent_path))
        err::throw_with_trace("Path to torrent file provided does not exist.");

    std::ifstream torrent_istream(torrent_path.c_str());

    if (!torrent_istream)
        err::throw_with_trace(std::format("Failed to open torrent file: {}", torrent_path.string())
        );

    return parse_torrent_file(torrent_istream);
}

TorrentMetadata parse_torrent_file(const std::string& torrent_str) {
    std::istringstream torrent_istream{torrent_str};
    return parse_torrent_file(torrent_istream);
}

}  // namespace torrent::md
