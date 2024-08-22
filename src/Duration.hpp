#pragma once

#include <chrono>

namespace torrent::duration {

static constexpr std::chrono::seconds      HANDSHAKE_TIMEOUT{10};
static constexpr std::chrono::seconds      CONNECTION_TIMEOUT{10};
static constexpr std::chrono::seconds      SEND_MSG_TIMEOUT{10};
static constexpr std::chrono::seconds      RECEIVE_MSG_TIMEOUT{10};
static constexpr std::chrono::seconds      REQUEST_TIMEOUT{10};
static constexpr std::chrono::seconds      PEER_CLEANUP_INTERVAL{10};
static constexpr std::chrono::milliseconds REQUEST_INTERVAL{500};

}  // namespace torrent::duration
