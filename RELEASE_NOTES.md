WARP64U is the Teletype Games store on the C64. It reads the C64 shelf of `http://archive.teletypegames.org/C64/`, installs and updates the games on a disk drive, and starts them.

**Install:** copy `WARP64U` from `warp64u-<version>.prg` to a disk or an SD2IEC, and start it with `LOAD"WARP64U",8,1` and `RUN`. It needs a network: an Ultimate (C64 Ultimate, Ultimate 64, 1541 Ultimate-II+) with the Command Interface turned on, a SwiftLink with a WiFi modem, or an RR-Net or ETH64 cartridge.

**0.1.0** (the first release):

- The list of the shelf beside what is on the disk: installed, update, local only
- RETURN installs or updates a title: the download goes to NAME.TMP, is checked against the size and CRC-32 of INDEX.TXT, and only then replaces NAME; a broken download leaves the installed version as it was
- R starts a title (LOAD and RUN through the keyboard buffer), DEL removes it, F1 reads the shelf again, RUN/STOP stops a download
- Three network drivers:
  - SwiftLink + WiFi modem (Hayes `ATDT host:port`): tried in VICE against the live archive, installing and starting titles
  - ip65 for RR-Net and ETH64: tried in VICE as far as DHCP
  - the Ultimate Command Interface: written from the documentation, **not tried on hardware yet**
- WARP64U.CFG for another server, port, path, drive, SD2IEC folder or driver
