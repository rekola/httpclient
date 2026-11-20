#!/usr/bin/env python3
"""Starts a small HTTP server on this machine and runs the smoke program against it.

usage: run_smoke.py PROGRAM
The program gets the base URL of the plain server and of an HTTPS server with a self-signed certificate (an empty
string if openssl is not there to make the certificate, which is an error when HTTPCLIENT_REQUIRE_TLS_TEST is set);
its exit status is this script's.
"""
import os
import shutil
import ssl
import subprocess
import sys
import tempfile
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


class QuietServer(ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, request, client_address):  # a client that refuses the certificate is expected
        pass


def make_certificate(directory):
    """A self-signed certificate for 127.0.0.1 and localhost; (certfile, keyfile), or None without openssl."""
    openssl = shutil.which("openssl")
    if not openssl:
        return None
    cert, key = os.path.join(directory, "cert.pem"), os.path.join(directory, "key.pem")
    result = subprocess.run(
        [openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", key, "-out", cert, "-days", "2",
         "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1"],
        capture_output=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr.decode(errors="replace"))
        return None
    return cert, key


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    server = QuietServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = "http://127.0.0.1:%d" % server.server_address[1]

    directory = tempfile.mkdtemp()
    https_base = ""
    secure = None
    try:
        files = make_certificate(directory)
        if files:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(*files)
            secure = QuietServer(("127.0.0.1", 0), Handler)
            secure.socket = context.wrap_socket(secure.socket, server_side=True)
            threading.Thread(target=secure.serve_forever, daemon=True).start()
            https_base = "https://127.0.0.1:%d" % secure.server_address[1]
        elif os.environ.get("HTTPCLIENT_REQUIRE_TLS_TEST"):
            sys.stderr.write("no certificate: openssl is needed for the HTTPS tests\n")
            return 1
        return subprocess.run([sys.argv[1], base, https_base], timeout=50).returncode
    finally:
        server.shutdown()
        if secure:
            secure.shutdown()
        shutil.rmtree(directory, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
