#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Bencode {

struct BencodeItem;
using BencodeInt    = intmax_t;
using BencodeString = std::string;
using BencodeList   = std::vector<BencodeItem>;
using BencodeDict   = std::unordered_map<std::string, BencodeItem>;

static constexpr BencodeInt MAX_STRING_LEN{1 << 27};  // 128 MiB

class BencodeItem : public std::variant<BencodeInt, BencodeString, BencodeList, BencodeDict> {
    public:
        template <typename T>
        BencodeItem(T&& variant_val, size_t start_off, size_t end_off)
            : variant<BencodeInt, BencodeString, BencodeList, BencodeDict>(
                  std::forward<T>(variant_val)
              ),
              start_off(start_off),
              end_off(end_off) {}

        BencodeItem()                                   = default;
        BencodeItem(const BencodeItem& item)            = default;
        BencodeItem(BencodeItem&& item)                 = default;
        BencodeItem& operator=(const BencodeItem& item) = default;
        BencodeItem& operator=(BencodeItem&& item)      = default;
        ~BencodeItem()                                  = default;

        [[nodiscard]] size_t start() const { return start_off; }
        [[nodiscard]] size_t end() const { return end_off; }

    private:
        // offsets that disclose the location of the item in the stream
        size_t start_off{};
        size_t end_off{};
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
