# WE64 – the Teletype Games store on the C64

WE64 lists the C64 games of Teletype Games, installs and updates them on a disk drive over the network, and starts them: WEDOS for the C64. It reads the C64 directory of the WarpEngine of teletypegames.org over plain HTTP, `http://archive.teletypegames.org/C64/`, so it runs on any C64 with a network, not only on an Ultimate.

## What it needs

- **A network**, one of:
  - an **Ultimate**: C64 Ultimate, Ultimate 64 or 1541 Ultimate-II+, with the **Command Interface turned on** in its menu (it is off by default), on Ethernet or Wi-Fi. The Ultimate does TCP and DNS itself. *This driver is written from the Ultimate documentation and has not run on hardware yet.*
  - a **SwiftLink** (or Turbo232) cartridge at $DE00 with a **WiFi modem** that dials `ATDT host:port` (Zimodem, WiModem232 and the like), 38400 baud, RTS/CTS. Tried on a C64 Ultimate and in VICE.
  - an **RR-Net or ETH64** cartridge (or a clone: 64NIC+ and the like), with DHCP on the network. WE64 brings its own TCP/IP stack, [ip65](https://github.com/cc65/ip65).
- **A disk drive** the KERNAL can write: a 1541/1571/1581, an SD2IEC, a Pi1541, the drives of an Ultimate.

## Install and start

Copy `WE64` to a disk (from `we-c64-<version>.d64`, or the `.prg` with any tool), then:

```
LOAD"WE64",8,1
RUN
```

It finds the network (an Ultimate, then a modem, then an RR-Net/ETH64), reads the directory and shows it beside what is on the disk. When the directory has another WE64 than the one running, it offers to update itself first; once installed, the new one starts in its place.

| Key | |
|---|---|
| CRSR up/down | choose a title |
| RETURN | install, or update to the server's version |
| R | start the title: WE64 leaves, and LOAD and RUN go through the keyboard buffer |
| DEL | remove it from the disk (asks first) |
| U | update every title that has a new version (WE64 itself is offered at start) |
| SPACE | the details of the title: file, the directory's and the disk's version, size, author, category, the whole description |
| P | the preview: a picture of the title on the whole screen, any key returns |
| C | one category only: all, then each category of the directory in turn (a title without one is a *game*) |
| F1 | read the directory again (or try the network again) |
| RUN/STOP | stop a download; in the list, quit |
| Q | quit |

A title is *installed*, an *update* (the server has another version) or *local only* (on the disk, gone from the server).

## How it installs

1. `GET /C64/INDEX.TXT` over HTTP/1.0 with a `Host` header. Each line is `file|version|size|crc32|title|author|description|category|preview` (the WarpEngine directory format, whose header line is `#SHELF 2`; a version 1 line, without the last two fields, reads too). The list holds 24 titles; more are counted as skipped.
2. A title is downloaded to `NAME.TMP` on the disk as it arrives, with its CRC-32 computed on the way.
3. Only if the size and the CRC-32 match the index line: `S0:NAME`, then `R0:NAME=NAME.TMP`. Otherwise `NAME.TMP` is scratched and the installed version stays.
4. `WE64.IDX` (the same format, the titles installed here) is written last, through `WE64.IDN` and a rename.

The disk name is the directory name without `.PRG` (`ARIADNE.PRG` → `ARIADNE`). The 1541's `@0:` save-with-replace is not used.

## Previews

A non-empty `preview` field means `NAME.PVW` is in the directory beside `NAME.PRG`: a C64 bitmap the WarpEngine makes from the title's gallery image (3 header bytes: mode, background, border; the screen matrix; the color RAM in multicolor; the bitmap). WE64 shows it from VIC bank 3: the bitmap goes straight to $E000, under the KERNAL; the color RAM to $D800; the matrix waits on the blanked text screen and is moved to $D000, under the I/O area, once the download is over (I/O and ROMs off for the copy, an RTI behind the NMI vector meanwhile).

## Settings: WE64.CFG

An optional SEQ file next to WE64, `KEY=value` lines; a missing key keeps its default.

| Key | Default | |
|---|---|---|
| `HOST` | `archive.teletypegames.org` | |
| `PORT` | `80` | |
| `PATH` | `/C64` | the folder that holds `INDEX.TXT` |
| `NET` | `auto` | `uci` (Ultimate), `modem` (SwiftLink + WiFi modem), `ip65` (RR-Net/ETH64), `auto` (in that order) |
| `DRIVE` | the drive WE64 was loaded from | where titles are installed |
| `DIR` | – | an SD2IEC folder (`CD:` at start) |

## Building

```sh
make          # build/we64.prg and build/we64.d64 (cc65; ip65 from vendor/)
make test     # the host tests
make vice     # start the disk image in VICE
sh scripts/vice-modem.sh   # in VICE with a SwiftLink and a modem, on the real archive
```

Needs [cc65](https://cc65.github.io/) (2.18 or 2.19) and, for the disk image, VICE's `c1541`.

| Path | |
|---|---|
| `src/main.c` | the screen, the keys, the details, updating, starting a title |
| `src/preview.c` | downloading and showing a preview |
| `src/store.c` | the directory and the local index, install / update / remove |
| `src/http.c` | HTTP/1.0 GET, the body streamed to a sink |
| `src/directory.c` | `INDEX.TXT` lines |
| `src/config.c`, `src/crc32.c`, `src/text.c` | `WE64.CFG`, zlib's CRC-32, ASCII ↔ PETSCII |
| `src/net.h`, `src/net_uci.c`, `src/net_modem.c`, `src/net_ip65.c` | one TCP connection, three drivers |
| `src/disk.h`, `src/disk_cbm.c` | the KERNAL file calls; channel 15 stays open while files are |
| `host/` | the host versions of `net` and `disk`, the test server, the tests, `modem.py` (a Hayes modem for VICE) |
| `vendor/ip65` | ip65 (MPL 1.1), the commit in `vendor/ip65/VENDORED` |

Everything on the wire is ASCII, and cc65 stores string literals as PETSCII: literals that go out are converted (`ascii_copy`), and wire bytes are compared as numbers (`ASCII_BAR` and the rest), never as character literals. DOS commands (`s0:`, `r0:`, `cd:`) are written in lower case, which cc65 stores as the unshifted PETSCII the drive expects.

## Tests

- **Host** (`scripts/ci-test.sh`, CI on every push): the portable layers built with the system compiler against a socket driver and a folder for a disk, talking to `host/server.py`, an HTTP/1.0 server that serves a directory the tests write: install, update, a file that does not match its line, a 404, a full disk, local only, remove, a body without `Content-Length`.
- **VICE with a SwiftLink and a modem** (`scripts/vice-modem.sh`: VICE's RS232 line to `host/modem.py`, which dials the real `archive.teletypegames.org`): the directory, installing (the file on the disk is byte for byte the one on the archive), an update check, and starting the installed title, all with the live server.
- **VICE, no network**: the list from `WE64.IDX`, and starting a title through the keyboard buffer.
- **VICE with an RR-Net** (a VICE built with Ethernet, pcap on the Mac's Wi-Fi): the ip65 driver finds the card and gets an address by DHCP; the Wi-Fi access point does not deliver unicast replies to the emulated card's MAC address, so DNS times out there, and the whole download needs a wired interface.
- **Hardware**: the Ultimate driver, a real WiFi modem and a real RR-Net/ETH64 are still to be tried.

Auto-detection has to tell apart cards that share the I/O area: a SwiftLink at $DE00 looks like an ETH64 to ip65's probe, so the modem is asked first and counts only when a modem answers `AT` with `OK`; `$C9` at $DF1D can be open bus, so the Ultimate must also answer an IDENTIFY of its network target. Every wait on the Ultimate is bounded.

## Publishing

Like every other C64 title of Teletype Games: `.woodpecker/warp.yaml` is a `platform: c64` marker, and the WarpEngine of teletypegames.org serves the pipeline. `build: make` makes its build step run `make we-c64.prg` (cc65, in the `c64-builder` image) instead of assembling `main.asm` with ACME; the rest is the same: `commodore-pack` builds the browser and native players around the PRG, and the version in `metadata.json` (keep `src/version.h` the same) is uploaded and published on a push to master. The C64 directory then serves it as `WE64.PRG`, so WE64 can update itself: installing it over the running copy replaces the file on the disk; start it again.

The host tests run in the pipeline's `check` step (`make check`, in the `c64-builder` image).

## License

MIT (see `LICENSE`); `vendor/ip65` is under the Mozilla Public License 1.1.
