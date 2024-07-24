#include "Crypto.hpp"
#include "TorrentMetadata.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using namespace torrent::md;
using torrent::crypto::Sha1;

TEST_CASE("TorrentFile: parse_torrent_file", "[TorrentFile]") {
    SECTION("Single file") {
        std::string torrent_file =
            R"(d8:announce41:http://bttracker.debian.org:6969/announce7:comment35:"Debian CD from cdimage.debian.org"10:created by13:mktorrent 1.113:creation datei1719662085e4:infod6:lengthi661651456e4:name31:debian-12.6.0-amd64-netinst.iso12:piece lengthi262144e6:pieces0:e8:url-listl94:https://cdimage.debian.org/cdimage/release/12.6.0/amd64/iso-cd/debian-12.6.0-amd64-netinst.iso94:https://cdimage.debian.org/cdimage/archive/12.6.0/amd64/iso-cd/debian-12.6.0-amd64-netinst.isoee)";

        TorrentMetadata torrent = parse_torrent_file(torrent_file);

        std::filesystem::path cur_path{std::filesystem::current_path()};
        REQUIRE(torrent.announce == "http://bttracker.debian.org:6969/announce");
        REQUIRE(torrent.piece_hashes == "");
        REQUIRE(torrent.piece_length == 262'144);
        REQUIRE(torrent.files.size() == 1);
        REQUIRE(torrent.files[0].get_path() == cur_path / "debian-12.6.0-amd64-netinst.iso");
        REQUIRE(torrent.files[0].get_length() == 661'651'456);
        REQUIRE(torrent.files[0].get_start_off() == 0);

        std::array<uint8_t, 20> info_hash_arr{{0xa4, 0x04, 0x0d, 0xa2, 0x37, 0xa2, 0xf9,
                                               0x51, 0x3c, 0x4a, 0x61, 0xf7, 0x92, 0xfb,
                                               0x62, 0xa0, 0x5a, 0xc9, 0xd4, 0x36}};

        Sha1 info_hash{Sha1::from_raw_data(info_hash_arr)};
        REQUIRE(torrent.info_hash == info_hash);
    }

    SECTION("Multiple files") {
        std::string torrent_file =
            R"(d8:announce41:http://bttracker.debian.org:6969/announce7:comment35:"Debian CD from cdimage.debian.org"10:created by13:mktorrent 1.113:creation datei1719662085e4:infod5:filesld6:lengthi661651456e4:pathl7:iso_dir31:debian-12.6.0-amd64-netinst.isoeed6:lengthi1234e4:pathl5:file1eed6:lengthi1024e4:pathl4:dir14:dir24:dir35:file2eee4:name8:main_dir12:piece lengthi262144e6:pieces0:e8:url-listl94:https://cdimage.debian.org/cdimage/release/12.6.0/amd64/iso-cd/debian-12.6.0-amd64-netinst.iso94:https://cdimage.debian.org/cdimage/archive/12.6.0/amd64/iso-cd/debian-12.6.0-amd64-netinst.isoee)";

        TorrentMetadata torrent = parse_torrent_file(torrent_file);

        std::filesystem::path cur_path{std::filesystem::current_path()};
        REQUIRE(torrent.announce == "http://bttracker.debian.org:6969/announce");
        REQUIRE(torrent.piece_hashes == "");
        REQUIRE(torrent.piece_length == 262'144);
        REQUIRE(torrent.files.size() == 3);

        REQUIRE(
            torrent.files[0].get_path() ==
            cur_path / "main_dir" / "iso_dir" / "debian-12.6.0-amd64-netinst.iso"
        );
        REQUIRE(torrent.files[0].get_length() == 661'651'456);
        REQUIRE(torrent.files[0].get_start_off() == 0);

        REQUIRE(torrent.files[1].get_path() == cur_path / "main_dir" / "file1");
        REQUIRE(torrent.files[1].get_length() == 1'234);
        REQUIRE(torrent.files[1].get_start_off() == 661'651'456);

        REQUIRE(
            torrent.files[2].get_path() ==
            cur_path / "main_dir" / "dir1" / "dir2" / "dir3" / "file2"
        );
        REQUIRE(torrent.files[2].get_length() == 1'024);
        REQUIRE(torrent.files[2].get_start_off() == 661'652'690);

        std::array<uint8_t, 20> info_hash_arr{{0x5e, 0x60, 0xf1, 0xdc, 0xeb, 0xa3, 0xfa,
                                               0xde, 0x17, 0x28, 0xf1, 0x78, 0x1f, 0xab,
                                               0x33, 0x58, 0x89, 0x16, 0x72, 0x3b}};

        Sha1 info_hash{Sha1::from_raw_data(info_hash_arr)};
        REQUIRE(torrent.info_hash == info_hash);
    }
}
