#include "PeerManager.hpp"

#include "Duration.hpp"
#include "Logger.hpp"
#include "PeerConnection.hpp"
#include "Utils.hpp"

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
    std::scoped_lock lock(peer_connections_mutex_);

    for (const auto& peer : peers) {
        // Check if the peer is already connected
        if (peer_connections_.contains(peer)) {
            continue;
        }

        // Initially, none of the peers are reconnecting, so set the second element to false
        peer_connections_.emplace(
            peer, std::make_pair(peer::PeerConnection(peer_conn_ctx_, *piece_manager_, peer), false)
        );

        auto& peer_connection = peer_connections_.at(peer).first;

        co_spawn(
            utils_ctx_,
            peer_connection.connect(handshake_message_, info_hash_),
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
                    co_spawn(peer_conn_ctx_, peer_connection.run(), asio::detached);
                }

                if (--remaining_connections == 0) {
                    LOG_INFO("All peer connections established");
                }
            }
        );
    }
}

void PeerManager::start() {
    if (started_) {
        return;
    }
    // Run the peer connection context
    peer_connection_thread_ = std::jthread([this] { peer_conn_ctx_.run(); });
    // Run the utility context
    utils_thread_ = std::jthread([this] { utils_ctx_.run(); });
    // Start the cleanup task
    co_spawn(utils_ctx_, cleanup_peer_connections(), asio::detached);
    // Start the download completion task
    co_spawn(utils_ctx_, handle_download_completion(), asio::detached);

    started_ = true;
}

void PeerManager::stop() {
    if (!started_) {
        return;
    }
    // Reset the work guards
    peer_conn_work_guard_.reset();
    utils_work_guard_.reset();
    // Stop the contexts
    peer_conn_ctx_.stop();
    utils_ctx_.stop();
    started_ = false;
}

awaitable<void> PeerManager::handle_download_completion() {
    asio::steady_timer timer(co_await this_coro::executor);

    while (!piece_manager_->completed_thread_safe()) {
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

    // Start with a random backoff delay between 1 and 5 seconds and double it on each retry
    auto backoff_delay{std::chrono::seconds(utils::generate_random<uint32_t>(1, 5))};

    while (peer.get_retries_left() > 0) {
        co_await peer.connect(handshake_message_, info_hash_);

        // Break the loop if the connection is established or there are no more retries left
        if (peer.get_state() == peer::PeerState::CONNECTED || peer.get_retries_left() == 0) {
            break;
        }

        co_await asio::steady_timer(co_await this_coro::executor, backoff_delay)
            .async_wait(use_nothrow_awaitable);
        backoff_delay *= 2;
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
        std::unique_lock lock(peer_connections_mutex_, std::try_to_lock);
        if (!lock.owns_lock()) {
            continue;
        }

        for (auto it = peer_connections_.begin(); it != peer_connections_.end();) {
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
                    it = peer_connections_.erase(it);
                    break;
                case peer::PeerState::TIMED_OUT:
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
