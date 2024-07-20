#include "Bencode.hpp"

#include "Error.hpp"

#include <charconv>
#include <istream>
#include <sstream>
#include <string>
#include <utility>

namespace Bencode {

namespace {

    [[nodiscard]] BencodeItem parse(std::istream& input);

    [[nodiscard]] BencodeItem parse_string(std::istream& input) {
        std::streampos start_pos{input.tellg() - static_cast<std::streampos>(1)};
        // unget the first digit of the string length
        input.unget();

        std::string length_str;

        std::getline(input, length_str, ':');

        BencodeInt length{};
        auto [p, ec] =
            std::from_chars(length_str.data(), length_str.data() + length_str.size(), length);

        if (ec == std::errc() && p == length_str.data() + length_str.size()) {
            if (length > MAX_STRING_LEN)
                err::throw_with_trace("String length exceeds maximum allowed length.");

            std::string str;
            str.resize(length);
            input.read(str.data(), length);
            if (input.fail())
                err::throw_with_trace("Invalid string: not enough bytes provided");
            std::streampos end_pos{input.tellg() - static_cast<std::streampos>(1)};

            return BencodeItem{
                std::move(str), static_cast<size_t>(start_pos), static_cast<size_t>(end_pos)
            };
        }
        err::throw_with_trace("Invlaid string length.");
    }

    [[nodiscard]] BencodeItem parse_int(std::istream& input) {
        std::streampos start_pos{input.tellg() - static_cast<std::streampos>(1)};
        std::string    integer_str;

        std::getline(input, integer_str, 'e');

        if (!input.good())
            err::throw_with_trace("Invalid integer.");

        BencodeInt parsed_integer{};

        const char* end{integer_str.data() + integer_str.size()};

        auto [p, ec] = std::from_chars(integer_str.data(), end, parsed_integer);

        if (ec == std::errc() && p == end) {
            std::streampos end_pos{input.tellg() - static_cast<std::streampos>(1)};
            return BencodeItem(
                parsed_integer, static_cast<size_t>(start_pos), static_cast<size_t>(end_pos)
            );
        } else
            err::throw_with_trace("Invalid integer.");
    }

    [[nodiscard]] BencodeItem parse_list(std::istream& input) {
        std::streampos start_pos{input.tellg() - static_cast<std::streampos>(1)};
        BencodeList    parsed_list{};

        int next_ch{'e'};
        while (input.good() && (next_ch = input.get()) != 'e') {
            input.unget();
            parsed_list.push_back(parse(input));
        }

        if (next_ch != 'e')
            err::throw_with_trace("Invalid list: no end provided");

        std::streampos end_pos{input.tellg() - static_cast<std::streampos>(1)};
        return BencodeItem(
            std::move(parsed_list), static_cast<size_t>(start_pos), static_cast<size_t>(end_pos)
        );
    }

    [[nodiscard]] BencodeItem parse_dict(std::istream& input) {
        std::streampos start_pos{input.tellg() - static_cast<std::streampos>(1)};
        BencodeDict    parsed_dict{};

        int next_ch{'e'};
        while (input.good() && (next_ch = input.get()) != 'e') {
            input.unget();

            std::string key{std::get<BencodeString>(parse(input))};

            parsed_dict.insert({key, parse(input)});
        }

        if (next_ch != 'e')
            err::throw_with_trace("Invalid dictionary: no end provided.");

        std::streampos end_pos{input.tellg() - static_cast<std::streampos>(1)};
        return BencodeItem(
            std::move(parsed_dict), static_cast<size_t>(start_pos), static_cast<size_t>(end_pos)
        );
    }

    [[nodiscard]] BencodeItem parse(std::istream& input) {
        int c = input.get();

        if (input.eof())
            err::throw_with_trace("Unexpected end of file. Expected a bencode value.");

        switch (c) {
            case 'i':
                return parse_int(input);
            case 'l':
                return parse_list(input);
            case 'd':
                return parse_dict(input);
            default:
                if (isdigit(c) != 0)
                    return parse_string(input);
                err::throw_with_trace("Invalid bencode value");
        }

        std::unreachable();
    }
}  // namespace

[[nodiscard]] BencodeItem BDecode(std::istream& input) {
    BencodeItem res = parse(input);
    if (input.peek() != EOF)
        err::throw_with_trace("Invalid bencode input. Unconsumed bytes left in the stream.");
    return res;
}

[[nodiscard]] BencodeItem BDecode(const std::string& input) {
    std::istringstream stream(input);
    return BDecode(stream);
}

}  // namespace Bencode
