"""The test server: a folder over HTTP/1.0, as nginx answers the C64.

    python3 host/server.py <root> <port> [address]

/C64/NOLENGTH.PRG is answered without a Content-Length, as an older or odder
server might; /C64/LEADING.PRG with an empty line before the status line, as a
modem's CONNECT line can leave one; /C64/TRAILING.PRG with NO CARRIER after
the body, as a modem adds when the server hangs up; everything else is a file of <root> with
its length."""
import http.server
import os
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def do_GET(self):
        if self.path.endswith("/NOLENGTH.PRG"):
            body = b"\x01\x08nolength"
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.end_headers()
            self.wfile.write(body)
            return
        if self.path.endswith("/TRAILING.PRG"):
            self.wfile.write(b"HTTP/1.0 200 OK\r\nContent-Length: 4\r\n\r\ntail\r\nNO CARRIER\r\n")
            return
        if self.path.endswith("/LEADING.PRG"):
            self.wfile.write(b"\r\nHTTP/1.0 200 OK\r\nContent-Length: 4\r\n\r\nlead")
            return
        if self.headers.get("Host") is None:
            self.send_error(400, "no Host header")
            return
        super().do_GET()

    def log_message(self, *args):
        pass


os.chdir(sys.argv[1])
address = sys.argv[3] if len(sys.argv) > 3 else "127.0.0.1"
http.server.ThreadingHTTPServer((address, int(sys.argv[2])), Handler).serve_forever()
