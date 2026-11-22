#!/bin/sh
# WE64 in VICE with a SwiftLink at $DE00 and a WiFi modem behind it: VICE's
# RS232 line goes to host/modem.py, a Hayes modem that dials ATDT host:port over
# TCP, so the emulated C64 reads the real http://archive.teletypegames.org.
# Needs Python 3 and VICE (x64sc, c1541); builds the program first.
#
#   sh scripts/vice-modem.sh             a fresh disk with WE64 on it
#   sh scripts/vice-modem.sh my.d64      a disk of your own (WE64 on it)
set -eu
cd "$(dirname "$0")/.."
make build/we64.prg >/dev/null
DISK="${1:-build/vice-modem.d64}"
if [ -z "${1:-}" ]; then
  rm -f "$DISK"
  c1541 -format "we64,64" d64 "$DISK" -write build/we64.prg we64 >/dev/null
fi
PORT="${MODEM_PORT:-6400}"
python3 host/modem.py "$PORT" &
MODEM=$!
trap 'kill "$MODEM" 2>/dev/null || true' EXIT
sleep 1
x64sc -silent +cart \
  -acia1 -acia1base 0xDE00 -acia1irq 1 -acia1mode 1 -myaciadev 0 \
  -rsdev1 "127.0.0.1:$PORT" -rsdev1baud 38400 +rsdev1ip232 \
  -autostart "$DISK"
