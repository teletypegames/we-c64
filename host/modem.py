"""A Hayes modem for VICE's SwiftLink, as a WiFi modem answers a C64.

    python3 host/modem.py [port]        (default 6400)

VICE connects its RS232 device to it (-rsdev1 127.0.0.1:6400, ip232 off). In
command mode it answers AT, ATE0/ATE1, ATZ, ATH and ATDT host:port, which opens
a TCP connection and switches to data mode (CONNECT 38400). In data mode the
bytes pass through; "+++" between two quiet seconds goes back to command mode,
and the remote side closing says NO CARRIER."""
import select
import socket
import sys
import time

GUARD = 1.0


class Modem:
    def __init__(self, c64):
        self.c64 = c64
        self.echo = True
        self.line = b""
        self.remote = None
        self.online = False
        self.last_c64 = time.monotonic()
        self.plus_at = None

    def reply(self, text):
        self.c64.sendall(b"\r\n" + text.encode() + b"\r\n")

    def hang_up(self):
        if self.remote:
            self.remote.close()
        self.remote = None
        self.online = False

    def command(self, command):
        print(f"modem: command {command!r}", flush=True)
        if not command.startswith(b"AT"):
            return self.reply("ERROR")
        if command == b"ATE0":
            self.echo = False
        elif command == b"ATE1":
            self.echo = True
        elif command.startswith(b"ATH"):
            self.hang_up()
        elif command.startswith(b"ATD"):
            target = command[4:] if command.startswith(b"ATDT") else command[3:]
            host, _, port = target.decode().strip().rpartition(":")
            try:
                self.remote = socket.create_connection((host.lower(), int(port)), timeout=10)
                self.remote.settimeout(None)
                self.online = True
                print(f"modem: connected to {host}:{port}", flush=True)
                return self.reply("CONNECT 38400")
            except (OSError, ValueError) as error:
                print(f"modem: {target!r}: {error}", flush=True)
                self.hang_up()
                return self.reply("NO CARRIER")
        self.reply("OK")

    def from_c64(self, data, now):
        quiet_before = now - self.last_c64 >= GUARD
        self.last_c64 = now
        if self.online:
            if data == b"+++" and quiet_before:
                self.plus_at = now
                return
            if self.plus_at is not None:
                self.remote.sendall(b"+++")
                self.plus_at = None
            self.remote.sendall(data)
            return
        for byte in data:
            ch = bytes([byte])
            if self.echo:
                self.c64.sendall(ch)
            if ch in (b"\r", b"\n"):
                command, self.line = self.line.strip().upper(), b""
                if command:
                    self.command(command)
            else:
                self.line += ch

    def tick(self, now):
        if self.plus_at is not None and now - self.plus_at >= GUARD and now - self.last_c64 >= GUARD:
            self.plus_at = None
            self.online = False
            self.reply("OK")

    def from_remote(self):
        data = self.remote.recv(4096)
        if data:
            if self.online:
                self.c64.sendall(data)
            return
        print("modem: remote closed", flush=True)
        was_online = self.online
        self.hang_up()
        if was_online:
            self.reply("NO CARRIER")


def serve(c64):
    modem = Modem(c64)
    while True:
        sockets = [c64] + ([modem.remote] if modem.remote else [])
        ready, _, _ = select.select(sockets, [], [], 0.1)
        now = time.monotonic()
        if c64 in ready:
            data = c64.recv(4096)
            if not data:
                modem.hang_up()
                return
            modem.from_c64(data, now)
        if modem.remote and modem.remote in ready:
            modem.from_remote()
        modem.tick(now)


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 6400
    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", port))
    server.listen(1)
    print(f"modem: listening on 127.0.0.1:{port}", flush=True)
    while True:
        c64, _ = server.accept()
        print("modem: VICE connected", flush=True)
        try:
            serve(c64)
        except OSError as error:
            print(f"modem: {error}", flush=True)
        c64.close()
        print("modem: VICE left", flush=True)


if __name__ == "__main__":
    main()
