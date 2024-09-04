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

        /**
         * @brief Create a SHA-1 hash from raw data
         *
         * @param data A pointer to the raw data
         * @return The SHA-1 hash
         */
        [[nodiscard]] static Sha1 from_raw_data(const uint8_t* data) {
            return Sha1(std::span<const uint8_t, SHA1_SIZE>(data, SHA1_SIZE));
        }

        /**
         * @brief Create a SHA-1 hash from raw data
         *
         * @param data A span of the raw data
         * @return The SHA-1 hash
         */
        [[nodiscard]] static Sha1 from_raw_data(std::span<const uint8_t, SHA1_SIZE> data) {
            return Sha1(data);
        }

        /**
         * @brief Compute the SHA-1 hash of a span of data
         *
         * @param data A span of the data to hash
         * @return The SHA-1 hash
         */
        [[nodiscard]] static Sha1 digest(std::span<const uint8_t> data);

        /**
         * @brief Compute the SHA-1 hash of a buffer
         *
         * @param data A pointer to the buffer
         * @param count The number of bytes in the buffer
         * @return The SHA-1 hash
         */
        [[nodiscard]] static Sha1 digest(const uint8_t* data, size_t count);

        /**
         * @brief Get a view to the underlying hash
         *
         * @return A span of the hash
         */
        [[nodiscard]] std::span<const uint8_t, SHA1_SIZE> get() const { return hash; }

    private:
        explicit Sha1(std::span<const uint8_t, SHA1_SIZE> src) {
            std::copy(src.begin(), src.end(), hash.begin());
        }

        std::array<uint8_t, SHA1_SIZE> hash{};
};

}  // namespace torrent::crypto
