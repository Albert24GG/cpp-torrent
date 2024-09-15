#pragma once

#include <chrono>

namespace torrent::duration {

inline constexpr std::chrono::seconds      HANDSHAKE_TIMEOUT{15};
inline constexpr std::chrono::seconds      CONNECTION_TIMEOUT{15};
inline constexpr std::chrono::seconds      SEND_MSG_TIMEOUT{10};
inline constexpr std::chrono::seconds      RECEIVE_MSG_TIMEOUT{40};
inline constexpr std::chrono::seconds      REQUEST_TIMEOUT{5};
inline constexpr std::chrono::seconds      PEER_CLEANUP_INTERVAL{10};
inline constexpr std::chrono::milliseconds REQUEST_INTERVAL{100};
inline constexpr std::chrono::milliseconds PROGRESS_BAR_REFRESH_RATE{1'000};
inline constexpr std::chrono::seconds      UDP_TRACKER_TIMEOUT{60};

}  // namespace torrent::duration
