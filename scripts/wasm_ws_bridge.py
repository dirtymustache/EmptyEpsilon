#!/usr/bin/env python3
"""WebSocket-to-TCP bridge for the EmptyEpsilon wasm client.

browser wasm client <-> websocket bridge <-> native EmptyEpsilon server

Forwards binary WebSocket frames to the native TCP server and back.
Supports WSS via --tls-cert / --tls-key.
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import hashlib
import logging
import signal
import ssl
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
    idle_timeout: float,
    connection_counter: list[int],
    max_connections: int,
) -> None:
    peer = ws_writer.get_extra_info("peername")

    if connection_counter[0] >= max_connections:
        logging.warning("bridge: connection limit reached (%d), rejecting %s", max_connections, peer)
        try:
            await send_ws_close(ws_writer)
        except Exception:
            pass
        ws_writer.close()
        return

    connection_counter[0] += 1
    logging.info("bridge: websocket client connected from %s (%d/%d)", peer, connection_counter[0], max_connections)

    tcp_reader: Optional[asyncio.StreamReader] = None
    tcp_writer: Optional[asyncio.StreamWriter] = None
    try:
        await asyncio.wait_for(accept_websocket(ws_reader, ws_writer), timeout=10.0)
        logging.info("bridge: websocket upgrade complete")

        tcp_reader, tcp_writer = await asyncio.wait_for(
            asyncio.open_connection(target_host, target_port), timeout=10.0
        )
        logging.info("bridge: connected to native server %s:%d", target_host, target_port)

        ws_to_tcp = asyncio.create_task(pump_ws_to_tcp(ws_reader, ws_writer, tcp_writer))
        tcp_to_ws = asyncio.create_task(pump_tcp_to_ws(tcp_reader, ws_writer))

        done, pending = await asyncio.wait(
            {ws_to_tcp, tcp_to_ws},
            return_when=asyncio.FIRST_COMPLETED,
            timeout=idle_timeout if idle_timeout > 0 else None,
        )
        for task in pending:
            task.cancel()
        if not done:
            logging.info("bridge: idle timeout reached for %s", peer)
        else:
            for task in done:
                exc = task.exception()
                if exc:
                    raise exc
    except asyncio.TimeoutError:
        logging.info("bridge: timeout for %s", peer)
    except asyncio.IncompleteReadError:
        logging.info("bridge: connection closed by %s", peer)
    except WebSocketProtocolError as exc:
        logging.warning("bridge: websocket protocol error from %s: %s", peer, exc)
    except OSError as exc:
        logging.warning("bridge: socket error for %s: %s", peer, exc)
    finally:
        connection_counter[0] -= 1
        if tcp_writer is not None:
            tcp_writer.close()
            await tcp_writer.wait_closed()
        try:
            await send_ws_close(ws_writer)
        except Exception:
            pass
        ws_writer.close()
        await ws_writer.wait_closed()
        logging.info("bridge: client disconnected %s (%d/%d active)", peer, connection_counter[0], max_connections)


async def main_async(args: argparse.Namespace) -> None:
    ssl_context = None
    scheme = "ws"
    if args.tls_cert or args.tls_key:
        if not args.tls_cert or not args.tls_key:
            raise SystemExit("--tls-cert and --tls-key must be provided together")
        ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ssl_context.load_cert_chain(certfile=args.tls_cert, keyfile=args.tls_key)
        scheme = "wss"

    connection_counter: list[int] = [0]

    server = await asyncio.start_server(
        lambda reader, writer: handle_client(
            reader, writer,
            args.target_host, args.target_port,
            args.idle_timeout,
            connection_counter,
            args.max_connections,
        ),
        args.listen_host,
        args.listen_port,
        ssl=ssl_context,
    )

    for sock in server.sockets or []:
        logging.info("bridge: listening on %s://%s:%d", scheme, sock.getsockname()[0], sock.getsockname()[1])
    logging.info("bridge: forwarding to tcp://%s:%d", args.target_host, args.target_port)
    logging.info("bridge: max connections=%d idle_timeout=%ss", args.max_connections, args.idle_timeout)

    loop = asyncio.get_running_loop()
    stop_event = asyncio.Event()

    def _handle_signal() -> None:
        logging.info("bridge: shutdown signal received, stopping gracefully")
        stop_event.set()

    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, _handle_signal)

    async with server:
        await stop_event.wait()
        server.close()
        await server.wait_closed()

    logging.info("bridge: stopped")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="WebSocket-to-TCP bridge for EmptyEpsilon wasm clients.")
    parser.add_argument("--listen-host", default="0.0.0.0", help="WebSocket listen host (default: 0.0.0.0)")
    parser.add_argument("--listen-port", type=int, default=35667, help="WebSocket listen port")
    parser.add_argument("--target-host", default="127.0.0.1", help="Native EmptyEpsilon server host")
    parser.add_argument("--target-port", type=int, default=35666, help="Native EmptyEpsilon server TCP port")
    parser.add_argument("--max-connections", type=int, default=20, help="Max concurrent WebSocket connections (default: 20)")
    parser.add_argument("--idle-timeout", type=float, default=300.0, help="Seconds before idle connection is dropped (default: 300, 0=disabled)")
    parser.add_argument("--tls-cert", help="PEM certificate file to enable WSS")
    parser.add_argument("--tls-key", help="PEM private key file to enable WSS")
    parser.add_argument("--verbose", action="store_true", help="Enable debug logging")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(message)s",
        datefmt="%Y-%m-%dT%H:%M:%S",
    )
    asyncio.run(main_async(args))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
