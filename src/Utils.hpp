#pragma once

#include <bit>
#include <cstddef>
#include <cstdlib>

namespace torrent::utils {

/**
 * Convert the given value to big endian
 *
 * @param value  the value to convert
 * @return the value in big endian
 */
constexpr auto host_to_network_order(auto value) {
    if constexpr (std::endian::native == std::endian::big) {
        return value;
    } else {
        return std::byteswap(value);
    }
}

/**
 * Convert the given value to host endian
 *
 * @param value  the value to convert
 * @return the value in host endian
 */
constexpr auto network_to_host_order(auto value) {
    return host_to_network_order(value);
}

/**
 * Calculate the ceiling of the division of the given integers
 *
 * @param divident the divident
 * @param divisor  the divisor
 * @return the ceiling of the division
 */
constexpr auto ceil_div(std::integral auto divident, std::integral auto divisor) {
    return (divident / divisor) + (divident % divisor != 0);
}

/**
 * Calculate the next aligned value of the given value
 *
 * @param value      the value to align
 * @param alignment  the alignment
 * @return the next aligned value
 */
template <typename T, typename U = size_t>
    requires std::integral<T> && std::integral<U>
constexpr auto next_aligned(T value, U alignment = alignof(std::max_align_t)) {
    return (value + (alignment - 1)) & ~(alignment - 1);
}

}  // namespace torrent::utils
