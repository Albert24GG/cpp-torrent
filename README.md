# cpp-torrent

cpp-torrent is a simple bittorrent client written in C++.
It support only leeching (no seeding) and works with both TCP and UDP trackers. Multi-file torrents are supported.
This was my attempt of learning more about the bittorrent protocol and apply some modern C++2x features.

## Requirements

- c++23 compiler (e.g. gcc 14)
- xmake

## Build

```bash
git clone https://github.com/Albert24GG/cpp-torrent.git
cd cpp-torrent
xmake config -m release
xmake
```

Optionally, you can copy to the current directory. Example:

```bash
cp ./build/linux/x86_64/release/cpp-torrent .
```

## Usage

```bash
Usage: cpp-torrent [--help] [--version] [--output-dir VAR] [--logging] [--log-file VAR] torrent_file

Positional arguments:
torrent_file      Path to the .torrent file

Optional arguments:
-h, --help        shows help message and exits
-v, --version     prints version information and exits
-o, --output-dir  Output directory [nargs=0..1] [default: "."]
-l, --logging     Enable logging
-lf, --log-file   Path to the log file [nargs=0..1] [default: "./log.txt"]
```

## Example

```bash
./cpp-torrent debian-12.6.0-amd64-netinst.iso.torrent
Torrent name: debian-12.6.0-amd64-netinst.iso
Total size: 661651456 B
[███████████████████████████████████▌             ] 73%  14.32 MiB/s | ETA: 11s | Peers: 35

```
