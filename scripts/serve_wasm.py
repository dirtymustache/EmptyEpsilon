#!/usr/bin/env python3
"""Small cache-friendly HTTP server for local EmptyEpsilon wasm testing."""

from __future__ import annotations

import argparse
import functools
import http.server
import mimetypes
import ssl
import urllib.error
import urllib.request
from pathlib import Path


IMMUTABLE_SUFFIXES = {".png", ".jpg", ".jpeg", ".ogg", ".wav"}


class WasmRequestHandler(http.server.SimpleHTTPRequestHandler):
    proxy_base_url = ""

    def end_headers(self) -> None:
        path = Path(self.translate_path(self.path))
        suffix = path.suffix.lower()
        if suffix in IMMUTABLE_SUFFIXES:
            self.send_header("Cache-Control", "public, max-age=31536000, immutable")
        elif suffix in {".data", ".wasm", ".js", ".html"}:
            self.send_header("Cache-Control", "no-store, max-age=0, must-revalidate")
            self.send_header("Pragma", "no-cache")
            self.send_header("Expires", "0")
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()

    def do_GET(self) -> None:
        if self.proxy_base_url and self.path.startswith("/admin-api/"):
            self._proxy_request("GET")
            return
        super().do_GET()

    def do_POST(self) -> None:
        if self.proxy_base_url and self.path.startswith("/admin-api/"):
            self._proxy_request("POST")
            return
        super().do_POST()

    def _proxy_request(self, method: str) -> None:
        target_path = self.path.removeprefix("/admin-api")
        target_url = self.proxy_base_url.rstrip("/") + target_path
        content_length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(content_length) if content_length else None
        request = urllib.request.Request(target_url, data=body, method=method)
        content_type = self.headers.get("Content-Type")
        if content_type:
            request.add_header("Content-Type", content_type)
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                payload = response.read()
                self.send_response(response.status)
                response_content_type = response.headers.get("Content-Type")
                if response_content_type:
                    self.send_header("Content-Type", response_content_type)
                self.send_header("Cache-Control", "no-cache")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
        except urllib.error.HTTPError as error:
            payload = error.read()
            self.send_response(error.code)
            self.send_header("Content-Type", error.headers.get("Content-Type", "text/plain; charset=utf-8"))
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
        except OSError as error:
            payload = f"proxy error: {error}".encode("utf-8")
            self.send_response(502)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)


def main() -> None:
    parser = argparse.ArgumentParser(description="Serve EmptyEpsilon wasm output with cache headers.")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=18086)
    parser.add_argument("--directory", default="build-wasm")
    parser.add_argument("--tls-cert", help="PEM certificate file to enable HTTPS")
    parser.add_argument("--tls-key", help="PEM private key file to enable HTTPS")
    parser.add_argument("--proxy-admin-base", default="http://127.0.0.1:8080", help="Optional base URL for proxying /admin-api/*")
    args = parser.parse_args()

    mimetypes.add_type("application/wasm", ".wasm")
    mimetypes.add_type("application/octet-stream", ".data")

    directory = str(Path(args.directory).resolve())
    WasmRequestHandler.proxy_base_url = args.proxy_admin_base.rstrip("/")
    handler = functools.partial(WasmRequestHandler, directory=directory)
    with http.server.ThreadingHTTPServer((args.host, args.port), handler) as httpd:
        scheme = "http"
        if args.tls_cert or args.tls_key:
            if not args.tls_cert or not args.tls_key:
                raise SystemExit("--tls-cert and --tls-key must be provided together")
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(certfile=args.tls_cert, keyfile=args.tls_key)
            httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
            scheme = "https"
        print(f"serving {directory} on {scheme}://{args.host}:{args.port}")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
