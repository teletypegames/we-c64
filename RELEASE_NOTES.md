WARP64U is the Teletype Games store on the C64. It reads the C64 shelf of `http://archive.teletypegames.org/C64/`, installs and updates the games on a disk drive, and starts them.

**Install:** copy `WARP64U` from `warp64u-<version>.prg` to a disk or an SD2IEC, and start it with `LOAD"WARP64U",8,1` and `RUN`. It needs a network: an Ultimate (C64 Ultimate, Ultimate 64, 1541 Ultimate-II+) with the Command Interface turned on, a SwiftLink with a WiFi modem, or an RR-Net or ETH64 cartridge.

**1.0.1**:

- WARP64U.IDX is read before the network starts, and read again up to three times when the drive does not answer; a read that still fails says the drive's status on the message row and keeps the list as it was, instead of showing nothing installed
- Removing a title names the right one in the message

**1.0.0**:

- At start, a newer WARP64U on the shelf is offered; once installed, it starts in place of the old one
- U updates every title that has a new version on the shelf
- SPACE shows the details of a title: file, the shelf's and the disk's version, size, author, category, the whole description
- P shows the preview of a title, a C64 picture of it on the whole screen, made by the WarpEngine from the title's image
- C narrows the list to one category (game, demo, tool...)
- Reads version 2 of the shelf index (the category and the preview after the description); version 1 still reads
- The list holds 24 titles (was 40), to make room for the new fields and screens

**0.1.1**:

- Published like every other C64 title: besides the PRG on the C64 shelf, the catalog has WARP64U in the browser player and in the Linux, Windows and macOS players

**0.1.0** (the first release):

- The list of the shelf beside what is on the disk: installed, update, local only
- RETURN installs or updates a title: the download goes to NAME.TMP, is checked against the size and CRC-32 of INDEX.TXT, and only then replaces NAME; a broken download leaves the installed version as it was
- R starts a title (LOAD and RUN through the keyboard buffer), DEL removes it, F1 reads the shelf again, RUN/STOP stops a download
- Three network drivers:
  - SwiftLink + WiFi modem (Hayes `ATDT host:port`): tried in VICE against the live archive, installing and starting titles
  - ip65 for RR-Net and ETH64: tried in VICE as far as DHCP
  - the Ultimate Command Interface: written from the documentation, **not tried on hardware yet**
- WARP64U.CFG for another server, port, path, drive, SD2IEC folder or driver
