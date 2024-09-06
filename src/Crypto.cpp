#include "Crypto.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <openssl/sha.h>

namespace torrent::crypto {

bool operator==(const Sha1& h1, const Sha1& h2) {
    return std::equal(h1.hash_.begin(), h1.hash_.end(), h2.hash_.begin());
}

[[nodiscard]] Sha1 Sha1::digest(std::span<const uint8_t> data) {
    Sha1 digest{};

    SHA1(data.data(), data.size(), digest.hash_.data());

    return digest;
}

[[nodiscard]] Sha1 Sha1::digest(const uint8_t* data, size_t count) {
    return Sha1::digest(std::span<const uint8_t>(data, count));
}

}  // namespace torrent::crypto
