#include "Bencode.hpp"
#include "Error.h"

#include <charconv>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Bencode {

namespace {

[[nodiscard]] BencodeItem parse(std::istream& input);

[[nodiscard]] BencodeItem parse_string(std::istream& input) {
    // unget the first digit of the string length
    input.unget();

    std::string length_str;

    std::getline(input, length_str, ':');

    BencodeInt length{};
    auto [p, ec] =
        std::from_chars(length_str.data(), length_str.data() + length_str.size(), length);

    if (ec == std::errc() && p == length_str.data() + length_str.size()) {
        if (length > MAX_STRING_LEN)
            throw std::runtime_error(get_err_msg("String length exceeds maximum allowed length."));

        std::string str;
        str.resize(length);
        input.read(str.data(), length);
        if (input.fail())
            throw std::runtime_error(get_err_msg("Invalid string: not enough bytes provided"));
        return BencodeItem{std::move(str)};
    }
    throw std::runtime_error(get_err_msg("Invalid string length."));
}

[[nodiscard]] BencodeItem parse_int(std::istream& input) {
    std::string integer_str;

    std::getline(input, integer_str, 'e');

    BencodeInt parsed_integer{};

    const char* end{integer_str.data() + integer_str.size()};

    auto [p, ec] = std::from_chars(integer_str.data(), end, parsed_integer);

    if (ec == std::errc() && p == end)
        return BencodeItem(parsed_integer);
    else
        throw std::runtime_error(get_err_msg("Invalid integer."));
}

[[nodiscard]] BencodeItem parse_list(std::istream& input) {
    BencodeList parsed_list{};

    int next_ch{'e'};
    while (input.good() && (next_ch = input.get()) != 'e') {
        input.unget();
        parsed_list.push_back(parse(input));
    }

    if (next_ch != 'e')
        throw std::runtime_error(get_err_msg("Invalid list: no list end provided."));

    return BencodeItem(std::move(parsed_list));
}

[[nodiscard]] BencodeItem parse_dict(std::istream& input) {
    BencodeDict parsed_dict{};

    int next_ch{'e'};
    while (input.good() && (next_ch = input.get()) != 'e') {
        input.unget();

        std::string key{std::move(std::get<BencodeString>(parse(input)))};

        parsed_dict.insert({key, parse(input)});
    }

    if (next_ch != 'e')
        throw std::runtime_error(get_err_msg("Invalid dictionary: no dictionary end provided."));

    return BencodeItem(std::move(parsed_dict));
}

[[nodiscard]] BencodeItem parse(std::istream& input) {
    int c = input.get();

    if (input.eof())
        throw std::runtime_error(get_err_msg("Unexpected end of file. Expected a bencode value."));

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
        throw std::runtime_error(get_err_msg("Invalid bencode value."));
    }
}
}  // namespace

[[nodiscard]] BencodeItem BDecode(std::istream& input) {
    return parse(input);
}

[[nodiscard]] BencodeItem BDecode(const std::string& input) {
    std::istringstream stream(input);
    return parse(stream);
}

}  // namespace Bencode
