#include <iostream>

#include "Bencode.hpp"

int main(int argc, char** argv) {
    std::string test{"l4:spami4s2ee"};

    Bencode::BencodeList res = std::get<Bencode::BencodeList>(Bencode::BDecode(test));

    std::cout << std::get<Bencode::BencodeInt>(res[1]);

    return 0;
}
