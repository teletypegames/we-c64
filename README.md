# WARP64U – the Teletype Games store on the C64

WARP64U lists the C64 games of Teletype Games, installs and updates them on a disk drive over the network, and starts them: DOSINST for the C64. It reads the C64 shelf of the WarpEngine of teletypegames.org over plain HTTP, `http://archive.teletypegames.org/C64/`, so it runs on any C64 with a network, not only on an Ultimate.

## What it needs

- **A network**, one of:
  - an **Ultimate**: C64 Ultimate, Ultimate 64 or 1541 Ultimate-II+, with the **Command Interface turned on** in its menu (it is off by default), on Ethernet or Wi-Fi. The Ultimate does TCP and DNS itself. *This driver is written from the Ultimate documentation and has not run on hardware yet.*
  - a **SwiftLink** (or Turbo232) cartridge at $DE00 with a **WiFi modem** that dials `ATDT host:port` (Zimodem, WiModem232 and the like), 38400 baud, RTS/CTS.
  - an **RR-Net or ETH64** cartridge (or a clone: 64NIC+ and the like), with DHCP on the network. WARP64U brings its own TCP/IP stack, [ip65](https://github.com/cc65/ip65).
- **A disk drive** the KERNAL can write: a 1541/1571/1581, an SD2IEC, a Pi1541, the drives of an Ultimate.

## Install and start

Copy `WARP64U` to a disk (from `warp64u-<version>.d64`, or the `.prg` with any tool), then:

```
LOAD"WARP64U",8,1
RUN
```

It finds the network (an Ultimate, then a modem, then an RR-Net/ETH64), reads the shelf and shows it beside what is on the disk:

| Key | |
|---|---|
| CRSR up/down | choose a title |
| RETURN | install, or update to the server's version |
| R | start the title: WARP64U leaves, and LOAD and RUN go through the keyboard buffer |
| DEL | remove it from the disk (asks first) |
| F1 | read the shelf again (or try the network again) |
| RUN/STOP | stop a download; in the list, quit |
| Q | quit |

A title is *installed*, an *update* (the server has another version) or *local only* (on the disk, gone from the server).

## How it installs

1. `GET /C64/INDEX.TXT` over HTTP/1.0 with a `Host` header. Each line is `file|version|size|crc32|title|author|description` (the WarpEngine shelf format, `#SHELF 1`).
2. A title is downloaded to `NAME.TMP` on the disk as it arrives, with its CRC-32 computed on the way.
3. Only if the size and the CRC-32 match the index line: `S0:NAME`, then `R0:NAME=NAME.TMP`. Otherwise `NAME.TMP` is scratched and the installed version stays.
4. `WARP64U.IDX` (the same format, the titles installed here) is written last, through `WARP64U.IDN` and a rename.

The disk name is the shelf name without `.PRG` (`ARIADNE.PRG` → `ARIADNE`). The 1541's `@0:` save-with-replace is not used.

## Settings: WARP64U.CFG

An optional SEQ file next to WARP64U, `KEY=value` lines; a missing key keeps its default.

| Key | Default | |
|---|---|---|
| `HOST` | `archive.teletypegames.org` | |
| `PORT` | `80` | |
| `PATH` | `/C64` | the folder that holds `INDEX.TXT` |
| `NET` | `auto` | `uci` (Ultimate), `modem` (SwiftLink + WiFi modem), `ip65` (RR-Net/ETH64), `auto` (in that order) |
| `DRIVE` | the drive WARP64U was loaded from | where titles are installed |
| `DIR` | – | an SD2IEC folder (`CD:` at start) |

## Building

```sh
make          # build/warp64u.prg and build/warp64u.d64 (cc65; ip65 from vendor/)
make test     # the host tests
make vice     # start the disk image in VICE
sh scripts/vice-modem.sh   # in VICE with a SwiftLink and a modem, on the real archive
```

Needs [cc65](https://cc65.github.io/) (2.18 or 2.19) and, for the disk image, VICE's `c1541`.

| Path | |
|---|---|
| `src/main.c` | the screen, the keys, starting a title |
| `src/store.c` | the shelf and the local index, install / update / remove |
| `src/http.c` | HTTP/1.0 GET, the body streamed to a sink |
| `src/shelf.c` | `INDEX.TXT` lines |
| `src/config.c`, `src/crc32.c`, `src/text.c` | `WARP64U.CFG`, zlib's CRC-32, ASCII ↔ PETSCII |
| `src/net.h`, `src/net_uci.c`, `src/net_modem.c`, `src/net_ip65.c` | one TCP connection, three drivers |
| `src/disk.h`, `src/disk_cbm.c` | the KERNAL file calls; channel 15 stays open while files are |
| `host/` | the host versions of `net` and `disk`, the test server, the tests, `modem.py` (a Hayes modem for VICE) |
| `vendor/ip65` | ip65 (MPL 1.1), the commit in `vendor/ip65/VENDORED` |

Everything on the wire is ASCII, and cc65 stores string literals as PETSCII: literals that go out are converted (`ascii_copy`), and wire bytes are compared as numbers (`ASCII_BAR` and the rest), never as character literals. DOS commands (`s0:`, `r0:`, `cd:`) are written in lower case, which cc65 stores as the unshifted PETSCII the drive expects.

## Tests

- **Host** (`scripts/ci-test.sh`, CI on every push): the portable layers built with the system compiler against a socket driver and a folder for a disk, talking to `host/server.py`, an HTTP/1.0 server that serves a shelf the tests write: install, update, a file that does not match its line, a 404, a full disk, local only, remove, a body without `Content-Length`.
- **VICE with a SwiftLink and a modem** (`scripts/vice-modem.sh`: VICE's RS232 line to `host/modem.py`, which dials the real `archive.teletypegames.org`): the shelf, installing (the file on the disk is byte for byte the one on the archive), an update check, and starting the installed title, all with the live server.
- **VICE, no network**: the list from `WARP64U.IDX`, and starting a title through the keyboard buffer.
- **VICE with an RR-Net** (a VICE built with Ethernet, pcap on the Mac's Wi-Fi): the ip65 driver finds the card and gets an address by DHCP; the Wi-Fi access point does not deliver unicast replies to the emulated card's MAC address, so DNS times out there, and the whole download needs a wired interface.
- **Hardware**: the Ultimate driver, a real WiFi modem and a real RR-Net/ETH64 are still to be tried.

Auto-detection has to tell apart cards that share the I/O area: a SwiftLink at $DE00 looks like an ETH64 to ip65's probe, so the modem is asked first and counts only when a modem answers `AT` with `OK`; `$C9` at $DF1D can be open bus, so the Ultimate must also answer an IDENTIFY of its network target. Every wait on the Ultimate is bounded.

## Publishing

`.woodpecker.yaml`: on a tag `vX.Y.Z` (the same as `src/version.h` and `metadata.json`), `scripts/ci-release.sh` makes the Gitea release with `warp64u-<version>.prg` and `RELEASE_NOTES.md`, and publishes the same file to the WarpEngine of teletypegames.org (platform `c64`, `shelf_name` `WARP64U`), so WARP64U is on its own shelf and can update itself: installing it over the running copy replaces the file on the disk; start it again.

## License

MIT (see `LICENSE`); `vendor/ip65` is under the Mozilla Public License 1.1.
