#!/usr/bin/env python3
"""Minimal WebSocket-to-TCP bridge for the EmptyEpsilon wasm client.

This is a small local prototype intended for browser testing:

browser wasm client <-> websocket bridge <-> native EmptyEpsilon server

It supports a single websocket hop and forwards binary messages to the native
TCP server as raw payloads. It is deliberately minimal and is not meant to be
internet-exposed as-is.
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import hashlib
import logging
import struct
from typing import Optional


GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

CMD_AUDIO_COMM_START = 0x0020
CMD_AUDIO_COMM_DATA = 0x0021
CMD_AUDIO_COMM_STOP = 0x0022


def describe_payload(payload: bytes) -> str:
    if not payload:
        return "0 bytes"
    preview = " ".join(f"{value:02x}" for value in payload[:8])
    if len(payload) > 8:
        preview += " ..."
    return f"{len(payload)} bytes [{preview}]"


def try_parse_packet_length(buffer: bytearray) -> Optional[tuple[int, int]]:
    value = 0
    prefix_length = 0
    for raw_byte in buffer:
        prefix_length += 1
        value = (value << 7) | (raw_byte & 0x7F)
        if not (raw_byte & 0x80):
            return prefix_length, value
    return None


def describe_command(command: int) -> str:
    if command == CMD_AUDIO_COMM_START:
        return "CMD_AUDIO_COMM_START"
    if command == CMD_AUDIO_COMM_DATA:
        return "CMD_AUDIO_COMM_DATA"
    if command == CMD_AUDIO_COMM_STOP:
        return "CMD_AUDIO_COMM_STOP"
    return f"cmd=0x{command:04x}"


def inspect_packet(payload: bytes) -> Optional[str]:
    parsed = try_parse_packet_length(bytearray(payload))
    if parsed is None:
        return None
    prefix_length, packet_length = parsed
    total_length = prefix_length + packet_length
    if total_length > len(payload) or packet_length < 2:
        return None
    command = struct.unpack_from("<H", payload, prefix_length)[0]
    if command not in {CMD_AUDIO_COMM_START, CMD_AUDIO_COMM_STOP}:
        return None
    detail = describe_command(command)
    if packet_length >= 6:
        client_id = struct.unpack_from("<i", payload, prefix_length + 2)[0]
        detail += f" client_id={client_id}"
    if command == CMD_AUDIO_COMM_START and packet_length >= 10:
        target_identifier = struct.unpack_from("<i", payload, prefix_length + 6)[0]
        detail += f" target={target_identifier}"
    return detail


class WebSocketProtocolError(RuntimeError):
    pass


async def read_http_request(reader: asyncio.StreamReader) -> tuple[str, dict[str, str]]:
    request_line = await reader.readline()
    if not request_line:
        raise WebSocketProtocolError("empty HTTP request")

    headers: dict[str, str] = {}
    while True:
        line = await reader.readline()
        if not line:
            raise WebSocketProtocolError("unexpected EOF in HTTP headers")
        if line in (b"\r\n", b"\n"):
            break
        name, _, value = line.decode("utf-8").partition(":")
        headers[name.strip().lower()] = value.strip()
    return request_line.decode("utf-8").rstrip(), headers


async def accept_websocket(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
    _, headers = await read_http_request(reader)
    key = headers.get("sec-websocket-key")
    upgrade = headers.get("upgrade", "").lower()
    connection = headers.get("connection", "").lower()

    if not key or upgrade != "websocket" or "upgrade" not in connection:
        raise WebSocketProtocolError("request is not a websocket upgrade")

    accept = base64.b64encode(hashlib.sha1((key + GUID).encode("ascii")).digest()).decode("ascii")
    response = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {accept}\r\n"
        "\r\n"
    )
    writer.write(response.encode("ascii"))
    await writer.drain()


async def recv_ws_frame(reader: asyncio.StreamReader) -> Optional[bytes]:
    header = await reader.readexactly(2)
    first, second = header[0], header[1]
    fin = (first & 0x80) != 0
    opcode = first & 0x0F
    masked = (second & 0x80) != 0
    length = second & 0x7F

    if not fin:
        raise WebSocketProtocolError("fragmented frames are not supported by this bridge")

    if length == 126:
        length = struct.unpack("!H", await reader.readexactly(2))[0]
    elif length == 127:
        length = struct.unpack("!Q", await reader.readexactly(8))[0]

    mask = await reader.readexactly(4) if masked else b""
    payload = await reader.readexactly(length) if length else b""

    if masked:
        payload = bytes(value ^ mask[index % 4] for index, value in enumerate(payload))

    if opcode == 0x8:
        return None
    if opcode == 0x9:
        raise WebSocketProtocolError("ping frames should be handled by caller")
    if opcode == 0xA:
        return b""
    if opcode != 0x2:
        raise WebSocketProtocolError(f"unsupported websocket opcode {opcode}")
    return payload


async def send_ws_frame(writer: asyncio.StreamWriter, payload: bytes, opcode: int = 0x2) -> None:
    header = bytearray()
    header.append(0x80 | (opcode & 0x0F))
    length = len(payload)
    if length < 126:
        header.append(length)
    elif length < (1 << 16):
        header.append(126)
        header.extend(struct.pack("!H", length))
    else:
        header.append(127)
        header.extend(struct.pack("!Q", length))
    writer.write(bytes(header) + payload)
    await writer.drain()


async def send_ws_close(writer: asyncio.StreamWriter) -> None:
    await send_ws_frame(writer, b"", opcode=0x8)


async def pump_ws_to_tcp(ws_reader: asyncio.StreamReader, ws_writer: asyncio.StreamWriter, tcp_writer: asyncio.StreamWriter) -> None:
    while True:
        first = await ws_reader.readexactly(1)
        second = await ws_reader.readexactly(1)
        first_byte = first[0]
        second_byte = second[0]
        fin = (first_byte & 0x80) != 0
        opcode = first_byte & 0x0F
        masked = (second_byte & 0x80) != 0
        length = second_byte & 0x7F

        if not fin:
            raise WebSocketProtocolError("fragmented frames are not supported by this bridge")

        if length == 126:
            length = struct.unpack("!H", await ws_reader.readexactly(2))[0]
        elif length == 127:
            length = struct.unpack("!Q", await ws_reader.readexactly(8))[0]

        mask = await ws_reader.readexactly(4) if masked else b""
        payload = await ws_reader.readexactly(length) if length else b""
        if masked:
            payload = bytes(value ^ mask[index % 4] for index, value in enumerate(payload))

        if opcode == 0x8:
            break
        if opcode == 0x9:
            await send_ws_frame(ws_writer, payload, opcode=0xA)
            continue
        if opcode == 0xA:
            continue
        if opcode != 0x2:
            raise WebSocketProtocolError(f"unsupported websocket opcode {opcode}")

        logging.info("bridge: ws -> tcp %s", describe_payload(payload))
        packet_info = inspect_packet(payload)
        if packet_info:
            logging.info("bridge: ws voice %s", packet_info)
        tcp_writer.write(payload)
        await tcp_writer.drain()


async def pump_tcp_to_ws(tcp_reader: asyncio.StreamReader, ws_writer: asyncio.StreamWriter) -> None:
    pending = bytearray()
    while True:
        chunk = await tcp_reader.read(65536)
        if not chunk:
            break
        pending.extend(chunk)
        while pending:
            parsed = try_parse_packet_length(pending)
            if parsed is None:
                break
            prefix_length, packet_length = parsed
            total_length = prefix_length + packet_length
            if len(pending) < total_length:
                break
            payload = bytes(pending[:total_length])
            del pending[:total_length]
            logging.info("bridge: tcp -> ws %s", describe_payload(payload))
            packet_info = inspect_packet(payload)
            if packet_info:
                logging.info("bridge: tcp voice %s", packet_info)
            await send_ws_frame(ws_writer, payload, opcode=0x2)


async def handle_client(
    ws_reader: asyncio.StreamReader,
    ws_writer: asyncio.StreamWriter,
    target_host: str,
    target_port: int,
) -> None:
    peer = ws_writer.get_extra_info("peername")
    logging.info("bridge: websocket client connected from %s", peer)
    tcp_reader: Optional[asyncio.StreamReader] = None
    tcp_writer: Optional[asyncio.StreamWriter] = None
    try:
        await accept_websocket(ws_reader, ws_writer)
        logging.info("bridge: websocket upgrade complete")

        tcp_reader, tcp_writer = await asyncio.open_connection(target_host, target_port)
        logging.info("bridge: connected to native server %s:%d", target_host, target_port)

        ws_to_tcp = asyncio.create_task(pump_ws_to_tcp(ws_reader, ws_writer, tcp_writer))
        tcp_to_ws = asyncio.create_task(pump_tcp_to_ws(tcp_reader, ws_writer))

        done, pending = await asyncio.wait(
            {ws_to_tcp, tcp_to_ws},
            return_when=asyncio.FIRST_COMPLETED,
        )
        for task in pending:
            task.cancel()
        for task in done:
            exc = task.exception()
            if exc:
                raise exc
    except asyncio.IncompleteReadError:
        logging.info("bridge: connection closed")
    except WebSocketProtocolError as exc:
        logging.warning("bridge: websocket protocol error: %s", exc)
    except OSError as exc:
        logging.warning("bridge: socket error: %s", exc)
    finally:
        if tcp_writer is not None:
            tcp_writer.close()
            await tcp_writer.wait_closed()
        try:
            await send_ws_close(ws_writer)
        except Exception:
            pass
        ws_writer.close()
        await ws_writer.wait_closed()
        logging.info("bridge: client disconnected")


async def main_async(args: argparse.Namespace) -> None:
    server = await asyncio.start_server(
        lambda reader, writer: handle_client(reader, writer, args.target_host, args.target_port),
        args.listen_host,
        args.listen_port,
    )

    for sock in server.sockets or []:
        logging.info("bridge: listening on ws://%s:%d", sock.getsockname()[0], sock.getsockname()[1])
    logging.info("bridge: forwarding to tcp://%s:%d", args.target_host, args.target_port)

    async with server:
        await server.serve_forever()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a local websocket-to-tcp bridge for EmptyEpsilon wasm testing.")
    parser.add_argument("--listen-host", default="127.0.0.1", help="WebSocket listen host")
    parser.add_argument("--listen-port", type=int, default=35667, help="WebSocket listen port")
    parser.add_argument("--target-host", default="127.0.0.1", help="Native EmptyEpsilon server host")
    parser.add_argument("--target-port", type=int, default=35666, help="Native EmptyEpsilon server TCP port")
    parser.add_argument("--verbose", action="store_true", help="Enable debug logging")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(message)s",
    )
    try:
        asyncio.run(main_async(args))
    except KeyboardInterrupt:
        logging.info("bridge: stopped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
