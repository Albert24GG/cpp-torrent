#pragma once

#include "Crypto.hpp"
#include "Error.hpp"
#include "ITracker.hpp"
#include "PeerInfo.hpp"

#include <asio.hpp>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace torrent {

class UdpTracker final : public ITracker {
    public:
        UdpTracker(
            const std::string& announce,
            crypto::Sha1       info_hash,
            std::string        client_id,
            uint16_t           client_port,
            size_t             torrent_size
        )
            : info_hash_(info_hash),
              torr_client_id_(std::move(client_id)),
              torr_client_port_(client_port),
              torrent_size_(torrent_size) {
            if (torr_client_id_.size() != 20) {
                err::throw_with_trace("Client ID must be 20 bytes long");
            }

            std::tie(host_, port_) = extract_url_info(announce);
        }

        /**
         * @brief Retrieves a list of peers from the tracker
         *
         * @param downloaded The number of bytes downloaded
         * @param uploaded The number of bytes uploaded
         * @return A list of peers if the request was successful, an empty optional otherwise
         */
        auto retrieve_peers(size_t downloaded, size_t uploaded = 0)
            -> std::optional<std::vector<PeerInfo>> override;

        /**
         * @brief Get the interval at which the client should poll the tracker
         *
         * @return  The interval
         */
        [[nodiscard]] auto get_interval() const -> std::chrono::seconds override {
            return interval_;
        }

    private:
        /**
         * @brief Extracts the host and port from the URL
         *
         * @param url The URL to extract the host and port from
         * @return A pair containing the host and port
         */
        auto extract_url_info(const std::string& url) -> std::pair<std::string, uint16_t>;

        /**
         * @brief Sends a connect request to the tracker
         *
         * @param socket The socket to send the request from
         * @param tracker_endpoint The endpoint of the tracker
         * @return The connection ID if the request was successful, an error code otherwise
         */
        auto send_connect_request(
            asio::ip::udp::socket& socket, const asio::ip::udp::endpoint& tracker_endpoint
        ) -> asio::awaitable<std::expected<uint64_t, std::error_code>>;

        /**
         * @brief Sends an announce request to the tracker
         *
         * @param socket The socket to send the request from
         * @param tracker_endpoint The endpoint of the tracker
         * @param connection_id The connection ID to use for the request
         * @return The list of peers if the request was successful, an error code otherwise
         */
        auto send_announce_request(
            asio::ip::udp::socket&         socket,
            const asio::ip::udp::endpoint& tracker_endpoint,
            uint64_t                       connection_id
        ) -> asio::awaitable<std::expected<std::vector<PeerInfo>, std::error_code>>;

        std::string host_;
        uint16_t    port_{};

        crypto::Sha1         info_hash_;
        std::string          torr_client_id_;
        uint16_t             torr_client_port_;
        uint64_t             uploaded_{0};
        uint64_t             downloaded_{};
        uint64_t             torrent_size_{};
        std::chrono::seconds interval_{};
};

}  // namespace torrent
