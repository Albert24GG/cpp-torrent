#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace torrent::crypto {

inline constexpr auto SHA1_SIZE = 20;

class Sha1 {
    public:
        Sha1()                       = default;
        Sha1(const Sha1&)            = default;
        Sha1(Sha1&&)                 = default;
        Sha1& operator=(const Sha1&) = default;
        Sha1& operator=(Sha1&&)      = default;
        ~Sha1()                      = default;

        friend bool operator==(const Sha1& h1, const Sha1& h2);

        [[nodiscard]] static Sha1 from_raw_data(const uint8_t* data) {
            return Sha1(std::span<const uint8_t, SHA1_SIZE>(data, SHA1_SIZE));
        }

        [[nodiscard]] static Sha1 from_raw_data(std::span<const uint8_t, SHA1_SIZE> data) {
            return Sha1(data);
        }

        [[nodiscard]] static Sha1 digest(std::span<const uint8_t> data);

        [[nodiscard]] static Sha1 digest(const uint8_t* data, size_t count);

        [[nodiscard]] std::span<const uint8_t, SHA1_SIZE> get() const { return hash; }

    private:
        explicit Sha1(std::span<const uint8_t, SHA1_SIZE> src) {
            std::copy(src.begin(), src.end(), hash.begin());
        }

        std::array<uint8_t, SHA1_SIZE> hash{};
};

bool operator==(const Sha1& h1, const Sha1& h2);

}  // namespace torrent::crypto
