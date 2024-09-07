#pragma once

#include "FileManager.hpp"
#include "PeerManager.hpp"
#include "PeerRetriever.hpp"
#include "PieceManager.hpp"
#include "TorrentMetadata.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace torrent {

class TorrentClient {
    public:
        TorrentClient(
            std::filesystem::path torrent_file,
            std::filesystem::path output_dir = ".",
            uint16_t              port       = 6'881
        );

        void start_download();

    private:
        md::TorrentMetadata              torrent_md_;
        std::shared_ptr<fs::FileManager> file_manager_;
        std::shared_ptr<PieceManager>    piece_manager_;
        std::shared_ptr<PeerManager>     peer_manager_;
        std::shared_ptr<PeerRetriever>   peer_retriever_;
};

}  // namespace torrent
