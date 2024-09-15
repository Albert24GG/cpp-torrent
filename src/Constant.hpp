#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace torrent {

// The base client ID for the torrent client
// The client ID is generated by appending 12 random digits to this base
inline constexpr std::string_view CLIENT_ID_BASE{"-qB4650-"};

inline constexpr uint32_t BLOCK_SIZE{1ULL << 14U};

inline constexpr uint32_t TRACKER_NUM_WANT{100U};

inline constexpr uint32_t TARGET_PEER_COUNT{30U};

inline constexpr uint32_t MAX_PEER_COUNT{50U};

inline constexpr size_t MAX_MEMPOOL_SIZE{1ULL << 29U};  // 512MB

namespace peer {
    inline constexpr uint32_t MAX_BLOCKS_IN_FLIGHT{10U};
    inline constexpr uint32_t MAX_BLOCKS_PER_REQUEST{5U};
    inline constexpr uint32_t MAX_RETRIES{3U};
}  // namespace peer

namespace crypto {
    inline constexpr uint32_t SHA1_SIZE{20};
}

namespace message {
    inline constexpr std::string_view PROTOCOL_IDENTIFIER{"BitTorrent protocol"};
    inline constexpr uint32_t         HANDSHAKE_MESSAGE_SIZE{68U};
    inline constexpr uint32_t         PROTOCOL_IDENTIFIER_SIZE{19U};
    inline constexpr uint32_t         RESERVED_SIZE{8U};
    inline constexpr uint32_t         INFO_HASH_SIZE{crypto::SHA1_SIZE};
    inline constexpr uint32_t         PEER_ID_SIZE{20U};
    inline constexpr uint32_t         MAX_SENT_MSG_SIZE{17U};

}  // namespace message

namespace ui {
    constexpr inline size_t           PROGRESS_BAR_WIDTH{50U};
    constexpr inline std::string_view PROGRESS_BAR_INIT_TEXT{"Initializing..."};

}  // namespace ui

}  // namespace torrent
