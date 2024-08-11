#include "PieceManager.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <thread>

using namespace torrent;
using namespace std::literals::chrono_literals;

namespace {

std::string read_from_file(const std::filesystem::path& path, size_t offset, size_t length) {
    std::ifstream file{path, std::ios::binary | std::ios::in};

    file.seekg(0, std::ios::end);
    if (file.tellg() < static_cast<std::streamoff>(offset + length)) {
        return "";
    }

    file.seekg(offset);
    std::string data;
    data.resize(length);
    file.read(data.data(), length);
    return data;
}

}  // namespace

TEST_CASE("PieceManager: Single Piece", "[PieceManager]") {
    static const std::array<torrent::md::FileInfo, 3> files_info{
        {{"file1", 0, BLOCK_SIZE},
         {"file2", BLOCK_SIZE, 2 * BLOCK_SIZE},
         {"file3", 3 * BLOCK_SIZE, 5 * BLOCK_SIZE / 2}}
    };
    std::shared_ptr<fs::FileManager> file_manager = std::make_shared<fs::FileManager>(files_info);

    std::string s1(BLOCK_SIZE, 'a');
    std::string s2{std::string(BLOCK_SIZE, 'b') + std::string(BLOCK_SIZE, 'c')};
    std::string s3{
        std::string(BLOCK_SIZE, 'd') + std::string(BLOCK_SIZE, 'e') +
        std::string(BLOCK_SIZE / 2, 'f')
    };

    std::string piece_data{s1 + s2 + s3};

    crypto::Sha1 piece_hash{
        crypto::Sha1::digest(reinterpret_cast<uint8_t*>(piece_data.data()), piece_data.size())
    };

    std::chrono::milliseconds request_timeout{10ms};

    PieceManager piece_manager(
        piece_data.size(), piece_data.size(), file_manager, piece_hash.get(), request_timeout
    );

    static constexpr std::array<std::byte, 1> peer1_bitfield{{std::byte{0b10000000}}};
    static constexpr std::array<std::byte, 1> peer2_bitfield{{std::byte{0b00000000}}};
    static constexpr std::array<std::byte, 1> peer3_bitfield{{std::byte{0b10000000}}};
    static constexpr std::array<std::byte, 1> peer4_bitfield{{std::byte{0b10000000}}};

    piece_manager.add_peer_bitfield(peer1_bitfield);
    piece_manager.add_peer_bitfield(peer2_bitfield);
    piece_manager.add_peer_bitfield(peer3_bitfield);
    piece_manager.add_peer_bitfield(peer4_bitfield);

    SECTION("Request block") {
        auto block = piece_manager.request_next_block(peer1_bitfield);

        // First block
        {
            REQUIRE(block.has_value());

            auto [index, offset, length] = block.value();
            REQUIRE(index == 0);
            REQUIRE(offset == 0);
            REQUIRE(length == BLOCK_SIZE);
        }

        // No block available: peer does not have the piece
        {
            block = piece_manager.request_next_block(peer2_bitfield);
            REQUIRE(!block.has_value());
        }

        // No block available: all pieces have been requested
        {
            for (auto i : std::views::iota(0, 5)) {
                block = piece_manager.request_next_block(peer1_bitfield);
                REQUIRE(block.has_value());
            }
            block = piece_manager.request_next_block(peer1_bitfield);
            REQUIRE(!block.has_value());
        }

        // Get same block after timeout
        {
            std::this_thread::sleep_for(request_timeout);
            block = piece_manager.request_next_block(peer1_bitfield);

            REQUIRE(block.has_value());

            auto [index, offset, length] = block.value();
            REQUIRE(index == 0);
            REQUIRE(offset == 0);
            REQUIRE(length == BLOCK_SIZE);
        }
    }

    SECTION("Receive invalid piece") {
        std::string_view block1{piece_data.data(), BLOCK_SIZE};
        std::string_view block2{piece_data.data() + BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block3{piece_data.data() + 2 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block4{piece_data.data() + 3 * BLOCK_SIZE, BLOCK_SIZE};
        std::string      block5{piece_data.data() + 4 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block6{piece_data.data() + 5 * BLOCK_SIZE, BLOCK_SIZE / 2};

        // Change block 5 by only one byte
        block5[0] = 'z';
        std::optional<std::tuple<uint32_t, size_t, size_t>> block;

        for (auto i : std::views::iota(0, 5)) {
            static_cast<void>(piece_manager.request_next_block(peer1_bitfield));
        }

        // Send the blocks in any order

        piece_manager.receive_block(
            0,
            std::span(reinterpret_cast<const std::byte*>(block6.data()), BLOCK_SIZE / 2),
            5 * BLOCK_SIZE
        );

        piece_manager.receive_block(
            0,
            std::span(reinterpret_cast<const std::byte*>(block3.data()), BLOCK_SIZE),
            2 * BLOCK_SIZE
        );

        piece_manager.receive_block(
            0,
            std::span(reinterpret_cast<const std::byte*>(block5.data()), BLOCK_SIZE),
            4 * BLOCK_SIZE
        );

        piece_manager.receive_block(
            0,
            std::span(reinterpret_cast<const std::byte*>(block4.data()), BLOCK_SIZE),
            3 * BLOCK_SIZE
        );

        piece_manager.receive_block(
            0, std::span(reinterpret_cast<const std::byte*>(block2.data()), BLOCK_SIZE), BLOCK_SIZE
        );

        piece_manager.receive_block(
            0, std::span(reinterpret_cast<const std::byte*>(block1.data()), BLOCK_SIZE), 0
        );

        std::string result_str{
            read_from_file(files_info[0].path, 0, BLOCK_SIZE) +
            read_from_file(files_info[1].path, 0, 2 * BLOCK_SIZE) +
            read_from_file(files_info[2].path, 0, 5 * BLOCK_SIZE / 2)
        };

        REQUIRE(result_str.size() == 0);
    }

    SECTION("Receive valid piece") {
        std::string_view block1{piece_data.data(), BLOCK_SIZE};
        std::string_view block2{piece_data.data() + BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block3{piece_data.data() + 2 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block4{piece_data.data() + 3 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block5{piece_data.data() + 4 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block6{piece_data.data() + 5 * BLOCK_SIZE, BLOCK_SIZE / 2};

        // Send block1
        auto block = piece_manager.request_next_block(peer1_bitfield);
        piece_manager.receive_block(
            0, std::span(reinterpret_cast<const std::byte*>(block1.data()), BLOCK_SIZE), 0
        );

        // Send block2
        block = piece_manager.request_next_block(peer1_bitfield);
        piece_manager.receive_block(
            0, std::span(reinterpret_cast<const std::byte*>(block2.data()), BLOCK_SIZE), BLOCK_SIZE
        );

        // Request remaining blocks
        block = piece_manager.request_next_block(peer1_bitfield);
        block = piece_manager.request_next_block(peer1_bitfield);
        block = piece_manager.request_next_block(peer1_bitfield);
        block = piece_manager.request_next_block(peer1_bitfield);

        // Send block6, block5, block3
        piece_manager.receive_block(
            0,
            std::span(reinterpret_cast<const std::byte*>(block6.data()), BLOCK_SIZE / 2),
            5 * BLOCK_SIZE
        );

        piece_manager.receive_block(
            0,
            std::span(reinterpret_cast<const std::byte*>(block3.data()), BLOCK_SIZE),
            2 * BLOCK_SIZE
        );

        piece_manager.receive_block(
            0,
            std::span(reinterpret_cast<const std::byte*>(block5.data()), BLOCK_SIZE),
            4 * BLOCK_SIZE
        );

        // let block4 timeout
        std::this_thread::sleep_for(request_timeout);

        // Send block4
        block = piece_manager.request_next_block(peer1_bitfield);
        piece_manager.receive_block(
            0,
            std::span(reinterpret_cast<const std::byte*>(block4.data()), BLOCK_SIZE),
            3 * BLOCK_SIZE
        );

        // Check if the piece has been completed correctly
        std::string result_str{
            read_from_file(files_info[0].path, 0, BLOCK_SIZE) +
            read_from_file(files_info[1].path, 0, 2 * BLOCK_SIZE) +
            read_from_file(files_info[2].path, 0, 5 * BLOCK_SIZE / 2)
        };

        REQUIRE(result_str == piece_data);
    }

    // Remove the files
    for (const auto& file : files_info) {
        std::filesystem::remove(file.path);
    }
}

TEST_CASE("PieceManager: Multiple Pieces", "[PieceManager]") {
    static const std::array<torrent::md::FileInfo, 3> files_info{
        {{"file1", 0, 2 * BLOCK_SIZE},
         {"file2", 2 * BLOCK_SIZE, 4 * BLOCK_SIZE},
         {"file3", 6 * BLOCK_SIZE, 21 * BLOCK_SIZE / 4}}
    };

    std::shared_ptr<fs::FileManager> file_manager = std::make_shared<fs::FileManager>(files_info);

    std::string s1{std::string(BLOCK_SIZE, 'a') + std::string(BLOCK_SIZE, 'b')};
    std::string s2{
        std::string(BLOCK_SIZE, 'c') + std::string(BLOCK_SIZE, 'd') + std::string(BLOCK_SIZE, 'e') +
        std::string(BLOCK_SIZE, 'f')
    };
    std::string s3{
        std::string(BLOCK_SIZE, 'g') + std::string(BLOCK_SIZE, 'h') + std::string(BLOCK_SIZE, 'i') +
        std::string(BLOCK_SIZE, 'j') + std::string(BLOCK_SIZE, 'k') +
        std::string(BLOCK_SIZE / 4, 'l')
    };

    std::string piece_data{s1 + s2 + s3};

    std::string piece_hashes;

    // Add the first 5 piece hashes
    for (auto i : std::views::iota(0, 5)) {
        crypto::Sha1 piece_hash{crypto::Sha1::digest(
            reinterpret_cast<uint8_t*>(piece_data.data() + static_cast<size_t>(i * 2) * BLOCK_SIZE),
            2 * BLOCK_SIZE
        )};
        piece_hashes.append(
            reinterpret_cast<const char*>(piece_hash.get().data()), crypto::SHA1_SIZE
        );
    }

    // Add the last piece hash
    crypto::Sha1 piece_hash{crypto::Sha1::digest(
        reinterpret_cast<uint8_t*>(piece_data.data() + 10 * BLOCK_SIZE), 5 * BLOCK_SIZE / 4
    )};

    piece_hashes.append(reinterpret_cast<const char*>(piece_hash.get().data()), crypto::SHA1_SIZE);

    std::chrono::milliseconds request_timeout{10ms};

    PieceManager piece_manager(
        2 * BLOCK_SIZE,
        piece_data.size(),
        file_manager,
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(piece_hashes.data()), piece_hashes.size()
        ),
        request_timeout
    );

    static constexpr std::array<std::byte, 1> peer1_bitfield{{std::byte{0b11111100}}};
    static constexpr std::array<std::byte, 1> peer2_bitfield{{std::byte{0b00000000}}};
    static constexpr std::array<std::byte, 1> peer3_bitfield{{std::byte{0b10000100}}};
    static constexpr std::array<std::byte, 1> peer4_bitfield{{std::byte{0b00110000}}};
    static constexpr std::array<std::byte, 1> peer5_bitfield{{std::byte{0b01010100}}};

    piece_manager.add_peer_bitfield(peer1_bitfield);
    piece_manager.add_peer_bitfield(peer2_bitfield);
    piece_manager.add_peer_bitfield(peer3_bitfield);
    piece_manager.add_peer_bitfield(peer4_bitfield);
    piece_manager.add_peer_bitfield(peer5_bitfield);

    piece_manager.add_available_piece(1);

    // Pieces ordering: 4, 0, 2, 1, 3, 5

    SECTION("Request blocks") {
        auto block = piece_manager.request_next_block(peer1_bitfield);

        // First block
        {
            REQUIRE(block.has_value());

            auto [index, offset, length] = block.value();
            REQUIRE(index == 4);
            REQUIRE(offset == 0);
            REQUIRE(length == BLOCK_SIZE);
        }

        // No block available: peer does not have the piece
        {
            block = piece_manager.request_next_block(peer2_bitfield);
            REQUIRE(!block.has_value());
        }

        // No block available: all pieces have been requested
        {
            for (auto i : std::views::iota(0, 11)) {
                block = piece_manager.request_next_block(peer1_bitfield);
                REQUIRE(block.has_value());
            }
            block = piece_manager.request_next_block(peer1_bitfield);
            REQUIRE(!block.has_value());
        }

        // Get block after timeout
        {
            std::this_thread::sleep_for(request_timeout);
            for (auto i : std::views::iota(0, 6)) {
                block = piece_manager.request_next_block(peer1_bitfield);
            }

            REQUIRE(block.has_value());

            auto [index, offset, length] = block.value();
            REQUIRE(index == 2);
            REQUIRE(offset == BLOCK_SIZE);
            REQUIRE(length == BLOCK_SIZE);
        }
    }

    SECTION("Receive pieces") {
        std::string_view block1{piece_data.data(), BLOCK_SIZE};
        std::string_view block2{piece_data.data() + BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block3{piece_data.data() + 2 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block4{piece_data.data() + 3 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block5{piece_data.data() + 4 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block6{piece_data.data() + 5 * BLOCK_SIZE, BLOCK_SIZE};
        std::string      block7{piece_data.data() + 6 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block8{piece_data.data() + 7 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block9{piece_data.data() + 8 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block10{piece_data.data() + 9 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block11{piece_data.data() + 10 * BLOCK_SIZE, BLOCK_SIZE};
        std::string_view block12{piece_data.data() + 11 * BLOCK_SIZE, BLOCK_SIZE / 4};

        // Change block 7 by only one byte

        for (auto i : std::views::iota(0, 12)) {
            static_cast<void>(piece_manager.request_next_block(peer1_bitfield));
        }

        // Send the blocks in any order

        piece_manager.receive_block(
            2, std::span(reinterpret_cast<const std::byte*>(block6.data()), BLOCK_SIZE), BLOCK_SIZE
        );

        piece_manager.receive_block(
            1, std::span(reinterpret_cast<const std::byte*>(block3.data()), BLOCK_SIZE), 0
        );

        piece_manager.receive_block(
            2, std::span(reinterpret_cast<const std::byte*>(block5.data()), BLOCK_SIZE), 0
        );

        piece_manager.receive_block(
            1, std::span(reinterpret_cast<const std::byte*>(block4.data()), BLOCK_SIZE), BLOCK_SIZE
        );

        piece_manager.receive_block(
            0, std::span(reinterpret_cast<const std::byte*>(block2.data()), BLOCK_SIZE), BLOCK_SIZE
        );

        piece_manager.receive_block(
            0, std::span(reinterpret_cast<const std::byte*>(block1.data()), BLOCK_SIZE), 0
        );

        piece_manager.receive_block(
            5,
            std::span(reinterpret_cast<const std::byte*>(block12.data()), BLOCK_SIZE / 4),
            BLOCK_SIZE
        );

        piece_manager.receive_block(
            4, std::span(reinterpret_cast<const std::byte*>(block9.data()), BLOCK_SIZE), 0
        );

        piece_manager.receive_block(
            3, std::span(reinterpret_cast<const std::byte*>(block8.data()), BLOCK_SIZE), BLOCK_SIZE
        );

        piece_manager.receive_block(
            4, std::span(reinterpret_cast<const std::byte*>(block10.data()), BLOCK_SIZE), BLOCK_SIZE
        );

        piece_manager.receive_block(
            5, std::span(reinterpret_cast<const std::byte*>(block11.data()), BLOCK_SIZE), 0
        );

        SECTION("All valid pieces") {
            piece_manager.receive_block(
                3, std::span(reinterpret_cast<const std::byte*>(block7.data()), BLOCK_SIZE), 0
            );

            std::string result_str{
                read_from_file(files_info[0].path, 0, 2 * BLOCK_SIZE) +
                read_from_file(files_info[1].path, 0, 4 * BLOCK_SIZE) +
                read_from_file(files_info[2].path, 0, 21 * BLOCK_SIZE / 4)
            };

            REQUIRE(result_str == piece_data);
        }

        SECTION("One invalid piece") {
            // Change block 7 by only one byte
            block7[0] = 'z';
            piece_manager.receive_block(
                3, std::span(reinterpret_cast<const std::byte*>(block7.data()), BLOCK_SIZE), 0
            );
            std::string result_str{
                read_from_file(files_info[0].path, 0, 2 * BLOCK_SIZE) +
                read_from_file(files_info[1].path, 0, 4 * BLOCK_SIZE) +
                read_from_file(files_info[2].path, 0, 21 * BLOCK_SIZE / 4)
            };

            REQUIRE(result_str != piece_data);
        }
    }

    // Remove the files
    for (const auto& file : files_info) {
        std::filesystem::remove(file.path);
    }
}
