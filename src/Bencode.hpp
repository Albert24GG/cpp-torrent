#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Bencode {

struct BencodeItem;
using BencodeInt = intmax_t;
using BencodeString = std::string;
using BencodeList = std::vector<BencodeItem>;
using BencodeDict = std::unordered_map<std::string, BencodeItem>;

static constexpr BencodeInt MAX_STRING_LEN{1 << 27};  // 128 MiB

struct BencodeItem : public std::variant<BencodeInt, BencodeString, BencodeList, BencodeDict> {
    using variant::variant;
};

/*
 * @brief Decode a bencoded item from the input stream
 * @param input Input stream
 * @return Decoded bencoded item
 */
[[nodiscard]] BencodeItem BDecode(std::istream& input);

/**
 * @brief Decode bencoded string
 * @param input Bencoded string
 * @return Decoded bencoded item
 */
[[nodiscard]] BencodeItem BDecode(const std::string& input);

}  // namespace Bencode
