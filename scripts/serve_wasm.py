#!/usr/bin/env python3
"""Small cache-friendly HTTP server for local EmptyEpsilon wasm testing."""

from __future__ import annotations

import argparse
import functools
import http.server
import mimetypes
from pathlib import Path


IMMUTABLE_SUFFIXES = {".png", ".jpg", ".jpeg", ".ogg", ".wav"}


class WasmRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self) -> None:
        path = Path(self.translate_path(self.path))
        suffix = path.suffix.lower()
        if suffix in IMMUTABLE_SUFFIXES:
            self.send_header("Cache-Control", "public, max-age=31536000, immutable")
        elif suffix in {".data", ".wasm", ".js"}:
            self.send_header("Cache-Control", "no-cache")
        elif suffix == ".html":
            self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()


def main() -> None:
    parser = argparse.ArgumentParser(description="Serve EmptyEpsilon wasm output with cache headers.")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=18086)
    parser.add_argument("--directory", default="build-wasm")
    args = parser.parse_args()

    mimetypes.add_type("application/wasm", ".wasm")
    mimetypes.add_type("application/octet-stream", ".data")

    directory = str(Path(args.directory).resolve())
    handler = functools.partial(WasmRequestHandler, directory=directory)
    with http.server.ThreadingHTTPServer((args.host, args.port), handler) as httpd:
        print(f"serving {directory} on http://{args.host}:{args.port}")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
