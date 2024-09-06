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

inline constexpr BencodeInt MAX_STRING_LEN{1ULL << 27U};  // 128 MiB

class BencodeItem : public std::variant<BencodeInt, BencodeString, BencodeList, BencodeDict> {
    public:
        template <typename T>
        explicit BencodeItem(T&& variant_val, size_t start_off = 0, size_t elem_len = 0)
            : variant<BencodeInt, BencodeString, BencodeList, BencodeDict>(
                  std::forward<T>(variant_val)
              ),
              start_off_(start_off),
              elem_len_(elem_len) {}

        BencodeItem()                                   = default;
        BencodeItem(const BencodeItem& item)            = default;
        BencodeItem(BencodeItem&& item)                 = default;
        BencodeItem& operator=(const BencodeItem& item) = default;
        BencodeItem& operator=(BencodeItem&& item)      = default;
        ~BencodeItem()                                  = default;

        /**
         * @brief Get the start offset of the bencoded item in the bencoded string
         *
         * @return The start offset
         */
        [[nodiscard]] size_t start() const { return start_off_; }

        /**
         * @brief Get the length of the bencoded item in the bencoded string
         *
         * @return The length
         */
        [[nodiscard]] size_t len() const { return elem_len_; }

    private:
        // start offset and length of the bencoded item in the input stream
        size_t start_off_{};
        size_t elem_len_{};
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
