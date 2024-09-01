#include "PeerManager.hpp"

#include "Duration.hpp"
#include "Logger.hpp"
#include "PeerConnection.hpp"

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <exception>
#include <thread>

using asio::awaitable;
using asio::co_spawn;
namespace this_coro = asio::this_coro;

// Use the nothrow awaitable completion token to avoid exceptions
constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(asio::use_awaitable);

namespace torrent {

void PeerManager::add_peers(std::span<PeerInfo> peers) {
    // Start the PeerManager if it hasn't been started yet
    start();

    auto remaining_connections{peers.size()};

    // Acquire the lock to protect the peer_connections map
    std::scoped_lock lock(peer_connections_mutex);

    for (const auto& peer : peers) {
        // Check if the peer is already connected
        if (peer_connections.contains(peer)) {
            continue;
        }

        // Initially, none of the peers are reconnecting, so set the second element to false
        peer_connections.emplace(
            peer, std::make_pair(peer::PeerConnection(peer_conn_ctx, *piece_manager, peer), false)
        );

        auto& peer_connection = peer_connections.at(peer).first;

        co_spawn(
            utils_ctx,
            peer_connection.connect(handshake_message, info_hash),
            [&](std::exception_ptr ep) {
                if (ep) {
                    try {
                        std::rethrow_exception(ep);
                    } catch (const std::exception& ep) {
                        LOG_ERROR(
                            "Failed to connect to peer {}:{} with error:\n{}",
                            peer.ip,
                            peer.port,
                            ep.what()
                        );
                    }
                } else if (peer_connection.get_state() == peer::PeerState::CONNECTED) {
                    co_spawn(peer_conn_ctx, peer_connection.run(), asio::detached);
                }

                if (--remaining_connections == 0) {
                    LOG_INFO("All peer connections established");
                }
            }
        );
    }
}

void PeerManager::start() {
    if (started) {
        return;
    }
    // Run the peer connection context
    peer_connection_thread = std::jthread([this] { peer_conn_ctx.run(); });
    // Run the utility context
    utils_thread = std::jthread([this] { utils_ctx.run(); });
    // Start the cleanup task
    co_spawn(utils_ctx, cleanup_peer_connections(), asio::detached);
    // Start the download completion task
    co_spawn(utils_ctx, handle_download_completion(), asio::detached);

    started = true;
}

void PeerManager::stop() {
    if (!started) {
        return;
    }
    // Reset the work guards
    peer_conn_work_guard.reset();
    utils_work_guard.reset();
    // Stop the contexts
    peer_conn_ctx.stop();
    utils_ctx.stop();
    started = false;
}

awaitable<void> PeerManager::handle_download_completion() {
    asio::steady_timer timer(co_await this_coro::executor);

    while (!piece_manager->completed()) {
        timer.expires_after(std::chrono::seconds(1));
        co_await timer.async_wait(use_nothrow_awaitable);
    }

    LOG_INFO("Download completed. Stopping the PeerManager");
    stop();
}

asio::awaitable<void> PeerManager::try_reconnection(
    const PeerInfo& peer_info, peer::PeerConnection& peer, bool& is_reconnecting
) {
    LOG_DEBUG("Trying to reconnect to peer {}:{}", peer_info.ip, peer_info.port);

    while (peer.get_state() != peer::PeerState::CONNECTED && peer.get_retries_left() > 0) {
        co_await peer.connect(handshake_message, info_hash);
    }

    if (peer.get_state() != peer::PeerState::CONNECTED) {
        LOG_ERROR(
            "Failed to reconnect to peer {}:{}. Removing the peer connection",
            peer_info.ip,
            peer_info.port
        );
        peer.disconnect();
    } else {
        co_spawn(co_await this_coro::executor, peer.run(), asio::detached);
    }

    is_reconnecting = false;
    co_return;
}

awaitable<void> PeerManager::cleanup_peer_connections() {
    // TODO: Change the condition to a stop flag
    while (true) {
        co_await asio::steady_timer(co_await this_coro::executor, duration::PEER_CLEANUP_INTERVAL)
            .async_wait(use_nothrow_awaitable);

        // Try to acquire the lock without blocking the thread
        std::unique_lock lock(peer_connections_mutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            continue;
        }

        for (auto it = peer_connections.begin(); it != peer_connections.end();) {
            auto& [peer_connection, is_reconnecting] = it->second;

            // Skip the peer if it's currently being reconnected
            if (is_reconnecting) {
                ++it;
                continue;
            }

            switch (peer_connection.get_state()) {
                case peer::PeerState::DISCONNECTED:
                    LOG_DEBUG(
                        "Removed peer {}:{} from the peer connections", it->first.ip, it->first.port
                    );
                    it = peer_connections.erase(it);
                    break;
                case peer::PeerState::TIMED_OUT:
                    // TODO: Maybe implement exponential backoff
                    is_reconnecting = true;
                    co_spawn(
                        co_await this_coro::executor,
                        try_reconnection(it->first, peer_connection, is_reconnecting),
                        asio::detached
                    );
                    break;
                default:
                    ++it;
            }
        }
    }
}  // namespace torrent

}  // namespace torrent
