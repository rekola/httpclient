#!/usr/bin/env python3
"""Starts a small HTTP server on this machine and runs the smoke program against it.

usage: run_smoke.py PROGRAM
The program gets the server's base URL as its argument; its exit status is this script's.
"""
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):  # quiet
        pass

    def reply(self, code, body=b"", headers=None, content_type="text/plain"):
        if isinstance(body, str):
            body = body.encode()
        try:
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            for k, v in (headers or {}).items():
                self.send_header(k, v)
            self.end_headers()
            self.wfile.write(body)
        except ConnectionError:  # the client gave up (a timeout, a refused body)
            pass

    def do_GET(self):
        path, _, query = self.path.partition("?")
        if path == "/hello":
            self.reply(200, "hello")
        elif path == "/status/404":
            self.reply(404, "nope")
        elif path == "/redirect":
            self.reply(302, "", {"Location": "/hello"})
        elif path == "/query":
            self.reply(200, query)
        elif path == "/headers":
            # What the server received, one "name: value" per line, names in lower case.
            self.reply(200, "".join("%s: %s\n" % (k.lower(), v) for k, v in self.headers.items()))
        elif path == "/large":
            self.reply(200, b"x" * 300000)
        elif path == "/slow":
            time.sleep(4)
            self.reply(200, "late")
        else:
            self.reply(404, "no such path")

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        if self.path == "/echo":
            self.reply(200, body, {"X-Seen-Content-Type": self.headers.get("Content-Type", "")})
        else:
            self.reply(404, "no such path")


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = "http://127.0.0.1:%d" % server.server_address[1]
    try:
        return subprocess.run([sys.argv[1], base], timeout=50).returncode
    finally:
        server.shutdown()


if __name__ == "__main__":
    sys.exit(main())
