#!/usr/bin/env python3
"""Starts a small HTTP server on this machine and runs the smoke program against it.

usage: run_smoke.py PROGRAM
The program gets the base URL of the plain server and of an HTTPS server with a self-signed certificate (an empty
string if openssl is not there to make the certificate, which is an error when HTTPCLIENT_REQUIRE_TLS_TEST is set);
its exit status is this script's.
"""
import os
import shutil
import gzip
import ssl
import subprocess
import sys
import tempfile
import zlib
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


# Bodies for the decompression tests. smoke.cpp makes the same bytes with the same rules (keep the two in step).
TEXT_BODY = b"The quick brown fox jumps over the lazy dog. " * 200  # 9000 bytes


def pseudo_random(n, seed=20240607):
    """n bytes of a 32-bit linear congruential generator (high byte of each state): reproducible in C++ too,
    and nearly incompressible, so the compressed body is about as large as the plain one."""
    out = bytearray()
    state = seed
    for _ in range(n):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        out.append(state >> 24)
    return bytes(out)


BIG_BODY = pseudo_random(400000)
BINARY_BODY = b"PK\x03\x04" + bytes(range(256)) * 20  # looks like a zip, was never encoded


def accepts(header, coding):
    """Whether an Accept-Encoding header value allows the coding (a q=0 forbids it)."""
    for part in (header or "").split(","):
        name, _, params = part.strip().partition(";")
        if name.strip().lower() == coding:
            return params.replace(" ", "").lower() not in ("q=0", "q=0.0", "q=0.00", "q=0.000")
    return False


def damaged(data, start):
    """The gzip stream with 64 bytes from start on flipped; it must really fail to decode."""
    data = bytearray(data)
    for i in range(start, start + 64):
        data[i] ^= 0xFF
    try:
        gzip.decompress(bytes(data))
    except Exception:
        return bytes(data)
    raise AssertionError("the damaged gzip body is still valid")


# Compressible text with the damage at the start of the deflate data, which breaks the stream itself (the decoder
# reports a format error, not just a wrong checksum). CORRUPT_PLAIN_SIZE is the length of the whole body before damage.
CORRUPT_PLAIN_SIZE = 300000
_text = b"".join(b"line %d of the corrupt body\n" % i for i in range(20000))[:CORRUPT_PLAIN_SIZE]
_gz = gzip.compress(_text, 6, mtime=0)
CORRUPT_GZIP = damaged(_gz, 12)  # in the first block's code-length table
# Incompressible data is stored as is, so the same damage leaves a decodable stream and only the CRC at the end fails.
_gz = gzip.compress(BIG_BODY[:100000], 6, mtime=0)
CORRUPT_CRC_GZIP = damaged(_gz, len(_gz) // 2)


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
        elif path == "/accept-encoding":
            # The Accept-Encoding the client sent ("" if none), plain.
            self.reply(200, self.headers.get("Accept-Encoding", ""))
        elif path == "/gzip":
            # gzip only if the request allows it; plain otherwise.
            if accepts(self.headers.get("Accept-Encoding"), "gzip"):
                self.reply(200, gzip.compress(TEXT_BODY, 6, mtime=0), {"Content-Encoding": "gzip"})
            else:
                self.reply(200, TEXT_BODY)
        elif path == "/deflate":
            # RFC 9110 "deflate": the zlib format (RFC 1950).
            self.reply(200, zlib.compress(TEXT_BODY), {"Content-Encoding": "deflate"})
        elif path == "/raw-deflate":
            # What some old servers send as "deflate": a bare RFC 1951 stream without the zlib wrapper.
            c = zlib.compressobj(6, zlib.DEFLATED, -15)
            self.reply(200, c.compress(TEXT_BODY) + c.flush(), {"Content-Encoding": "deflate"})
        elif path == "/gzip-large":
            self.reply(200, gzip.compress(BIG_BODY, 6, mtime=0), {"Content-Encoding": "gzip"})
        elif path == "/gzip-corrupt":
            self.reply(200, CORRUPT_GZIP, {"Content-Encoding": "gzip"})
        elif path == "/gzip-corrupt-crc":
            self.reply(200, CORRUPT_CRC_GZIP, {"Content-Encoding": "gzip"})
        elif path == "/binary":
            self.reply(200, BINARY_BODY, content_type="application/octet-stream")
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
