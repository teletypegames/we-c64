#!/bin/sh
# The host tests: the portable layers (shelf, config, http, store) built with the
# system C compiler against a socket driver and a folder for a disk, talking to a
# small HTTP/1.0 server (host/server.py) that serves a shelf the tests write.
#
#   scripts/ci-test.sh
set -eu

CC="${CC:-cc}"
WORK="$(mktemp -d)"
trap 'kill "${SERVER:-}" 2>/dev/null || true; rm -rf "$WORK"' EXIT
mkdir -p "$WORK/server/C64" "$WORK/disk"

"$CC" -std=c99 -Wall -Wextra -Werror -O1 -D_POSIX_C_SOURCE=200112L -o "$WORK/test_store" \
  host/test_store.c host/net_posix.c host/disk_posix.c \
  src/config.c src/crc32.c src/http.c src/shelf.c src/store.c src/text.c

PORT="${PORT:-18064}"
python3 host/server.py "$WORK/server" "$PORT" &
SERVER=$!
i=0
until python3 -c "import socket,sys; socket.create_connection(('127.0.0.1',$PORT),1)" 2>/dev/null; do
  i=$((i + 1)); [ "$i" -lt 50 ] || { echo "the test server did not start" >&2; exit 1; }
  sleep 0.1
done

SERVER_ROOT="$WORK/server" DISK_DIR="$WORK/disk" PORT="$PORT" "$WORK/test_store"
