#include "FileManager.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

namespace {

std::string read_from_file(const std::filesystem::path& path, size_t offset, size_t length) {
    std::ifstream file{path, std::ios::binary | std::ios::in};
    file.seekg(offset);
    std::string data;
    data.resize(length);
    file.read(data.data(), length);
    return data;
}

}  // namespace

TEST_CASE("FileManager: write", "[FileManager]") {
    static const std::array<torrent::md::FileInfo, 3> files_info{
        {{"file1", 0, 10}, {"file2", 10, 20}, {"file3", 30, 30}}
    };

    SECTION("Write individualy") {
        std::string str1(5, 'a');
        size_t      str1_offset{0};
        std::string str2(10, 'b');
        size_t      str2_offset{15};
        std::string str3(15, 'c');
        size_t      str3_offset{40};

        {
            torrent::fs::FileManager file_manager{files_info};
            file_manager.write(str1, str1_offset);
            file_manager.write(str2, str2_offset);
            file_manager.write(str3, str3_offset);
        }

        REQUIRE(
            read_from_file("file1", str1_offset - files_info[0].start_off, str1.size()) == str1
        );
        REQUIRE(
            read_from_file("file2", str2_offset - files_info[1].start_off, str2.size()) == str2
        );
        REQUIRE(
            read_from_file("file3", str3_offset - files_info[2].start_off, str3.size()) == str3
        );
    }

    SECTION("Write across files") {
        std::string str1(20, 'a');
        size_t      str1_offset{5};
        std::string str2(25, 'b');
        size_t      str2_offset{25};

        {
            torrent::fs::FileManager file_manager{files_info};
            file_manager.write(str1, str1_offset);
            file_manager.write(str2, str2_offset);
        }

        std::string read_str1 =
            read_from_file(
                "file1",
                str1_offset - files_info[0].start_off,
                files_info[0].start_off + files_info[0].length - str1_offset
            ) +
            read_from_file(
                "file2",
                0,
                str1.size() - (files_info[0].start_off + files_info[0].length - str1_offset)
            );
        REQUIRE(read_str1 == str1);
        std::string read_str2 =
            read_from_file(
                "file2",
                str2_offset - files_info[1].start_off,
                files_info[1].start_off + files_info[1].length - str2_offset
            ) +
            read_from_file(
                "file3",
                0,
                str2.size() - (files_info[1].start_off + files_info[1].length - str2_offset)
            );
        REQUIRE(read_str2 == str2);
    }
}
