#include "PeerManager.hpp"

#include "Duration.hpp"
#include "PeerConnection.hpp"

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <exception>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
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

    // Create the context for initiating the peer connections
    asio::io_context peer_init_ctx;

    auto                                                       remaining_connections{peers.size()};
    asio::executor_work_guard<asio::io_context::executor_type> init_work_guard{
        asio::make_work_guard(peer_init_ctx)
    };

    // Use a thread pool to initiate the peer connections
    asio::post(conn_init_thread_pool, [&peer_init_ctx] { peer_init_ctx.run(); });

    // Acquire the lock to protect the peer_connections map
    std::scoped_lock lock(peer_connections_mutex);

    for (const auto& peer : peers) {
        // Check if the peer is already connected
        if (peer_connections.contains(peer)) {
            continue;
        }

        peer_connections.emplace(peer, peer::PeerConnection(peer_conn_ctx, *piece_manager, peer));

        co_spawn(
            peer_init_ctx,
            peer_connections.at(peer).connect(handshake_message, info_hash),
            [&](std::exception_ptr ep) {
                if (ep) {
                    try {
                        std::rethrow_exception(ep);
                    } catch (const std::exception& ep) {
                        // spdlog::error("Failed to connect to peer: {}", ep.what());
                        spdlog::error(
                            "Failed to connect to peer {}:{} with error:\n{}",
                            peer.ip,
                            peer.port,
                            ep.what()
                        );
                    }
                }

                if (--remaining_connections == 0) {
                    spdlog::info("All peer connections established");
                    init_work_guard.reset();
                    peer_init_ctx.stop();
                }
            }
        );
    }

    conn_init_thread_pool.join();
}

void PeerManager::start() {
    if (started) {
        return;
    }
    // Run the peer connection context
    peer_connection_thread = std::jthread([this] { peer_conn_ctx.run(); });
    // Start the cleanup task
    co_spawn(peer_conn_ctx, cleanup_peer_connections(), asio::detached);
    // Start the download completion task
    co_spawn(peer_conn_ctx, handle_download_completion(), asio::detached);

    started = true;
}

void PeerManager::stop() {
    if (!started) {
        return;
    }
    // Reset the work guard to stop the peer connection context
    peer_conn_work_guard.reset();
    // Stop the peer connection context
    peer_conn_ctx.stop();
    started = false;
}

awaitable<void> PeerManager::handle_download_completion() {
    asio::steady_timer timer(co_await this_coro::executor);

    while (!piece_manager->completed()) {
        timer.expires_after(std::chrono::seconds(1));
        co_await timer.async_wait(use_nothrow_awaitable);
    }

    spdlog::info("Download completed. Stopping the PeerManager");
    stop();
}

awaitable<void> PeerManager::cleanup_peer_connections() {
    // Acquire the lock to protect the peer_connections map
    asio::steady_timer timer(co_await this_coro::executor);

    // TODO: Change the condition to a stop flag
    while (true) {
        timer.expires_after(duration::PEER_CLEANUP_INTERVAL);
        co_await timer.async_wait(use_nothrow_awaitable);

        // Try to acquire the lock without blocking the thread
        std::unique_lock lock(peer_connections_mutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            continue;
        }

        for (auto it = peer_connections.begin(); it != peer_connections.end();) {
            if (it->second.get_state() == peer::PeerState::DISCONNECTED) {
                spdlog::debug(
                    "Removed peer {}:{} from the peer connections", it->first.ip, it->first.port
                );
                it = peer_connections.erase(it);
            } else {
                ++it;
            }
        }
    }
}  // namespace torrent

}  // namespace torrent
