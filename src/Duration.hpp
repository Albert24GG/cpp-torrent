#pragma once

#include <chrono>

namespace torrent::duration {

inline constexpr std::chrono::seconds      HANDSHAKE_TIMEOUT{10};
inline constexpr std::chrono::seconds      CONNECTION_TIMEOUT{10};
inline constexpr std::chrono::seconds      SEND_MSG_TIMEOUT{10};
inline constexpr std::chrono::seconds      RECEIVE_MSG_TIMEOUT{20};
inline constexpr std::chrono::seconds      REQUEST_TIMEOUT{5};
inline constexpr std::chrono::seconds      PEER_CLEANUP_INTERVAL{10};
inline constexpr std::chrono::milliseconds REQUEST_INTERVAL{100};

}  // namespace torrent::duration
