#include "Logger.hpp"
#include "ProgressBar.hpp"
#include "TorrentClient.hpp"

#include <argparse/argparse.hpp>

int main(int argc, char** argv) {
    argparse::ArgumentParser arg_parser("cpp-torrent");

    arg_parser.add_argument("torrent_file").help("Path to the .torrent file");

    arg_parser.add_argument("-o", "--output-dir")
        .help("Output directory")
        .default_value(std::string("."));

    arg_parser.add_argument("-l", "--logging")
        .help("Enable logging")
        .default_value(false)
        .implicit_value(true);

    arg_parser.add_argument("-lf", "--log-file")
        .help("Path to the log file")
        .default_value(std::string("./log.txt"));

    try {
        arg_parser.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << arg_parser;
        return 1;
    }

    if (arg_parser.get<bool>("--logging")) {
        torrent::logger::init(arg_parser.get<std::string>("--log-file"));
#ifdef DEBUG
        torrent::logger::set_level(torrent::logger::Level::debug);
#else
        torrent::logger::set_level(torrent::logger::Level::info);
#endif
    } else {
        torrent::logger::set_level(torrent::logger::Level::off);
    }

    torrent::TorrentClient client(
        arg_parser.get<std::string>("torrent_file"), arg_parser.get<std::string>("--output-dir")
    );

    torrent::ui::ProgressBar progress_bar(client);

    std::jthread draw_thread([&progress_bar] { progress_bar.start_draw(); });

    client.start_download();

    progress_bar.stop_draw();

    return 0;
}
