#!/usr/bin/env python3
"""Build web-only derived asset copies using Pillow."""

from __future__ import annotations

import argparse
import fnmatch
import io
import json
import shutil
import struct
from pathlib import Path

from PIL import Image


LOOSE_IMAGE_RULES = [
    {"pattern": "resources/planets/*.png", "max_dim": 1536},
    {"pattern": "resources/skybox/*/*.png", "max_dim": 768},
    {"pattern": "resources/mesh/ship/Ender Battlecruiser.png", "max_dim": 1536},
    {"pattern": "resources/mesh/various/Shield bubble generator.jpg", "max_dim": 1536, "quality": 88},
    {"pattern": "resources/mesh/various/Shield bubble generator specular.jpg", "max_dim": 1536, "quality": 88},
]

PACK_IMAGE_RULES = {
    "Asteroids.pack": [
        {"pattern": "Astroid_*_d.png", "max_dim": 768},
        {"pattern": "Astroid_*_n.png", "max_dim": 768},
        {"pattern": "Astroid_*_s.png", "max_dim": 768},
    ],
    "msgamedev.pack": [
        {"pattern": "*/AlbedoAO/*.png", "max_dim": 768},
        {"pattern": "*PBRSpecular.png", "max_dim": 768},
    ],
}


def resize_dimensions(width: int, height: int, max_dim: int) -> tuple[int, int]:
    if max(width, height) <= max_dim:
        return width, height
    if width >= height:
        new_width = max_dim
        new_height = max(1, round(height * (max_dim / width)))
    else:
        new_height = max_dim
        new_width = max(1, round(width * (max_dim / height)))
    return new_width, new_height


def optimize_image_bytes(data: bytes, suffix: str, max_dim: int, quality: int | None = None) -> bytes:
    with Image.open(io.BytesIO(data)) as image:
        image.load()
        resized = image
        new_size = resize_dimensions(image.width, image.height, max_dim)
        if new_size != image.size:
            resized = image.resize(new_size, Image.Resampling.LANCZOS)

        output = io.BytesIO()
        suffix = suffix.lower()
        if suffix in {".jpg", ".jpeg"}:
            if resized.mode not in {"RGB", "L"}:
                resized = resized.convert("RGB")
            resized.save(output, format="JPEG", quality=quality or 88, optimize=True, progressive=True)
        elif suffix == ".png":
            save_image = resized
            if save_image.mode not in {"RGB", "RGBA", "L", "LA", "P"}:
                save_image = save_image.convert("RGBA")
            save_image.save(output, format="PNG", optimize=True, compress_level=9)
        else:
            return data
        optimized = output.getvalue()
        if new_size == image.size and len(optimized) >= len(data):
            return data
        return optimized


def match_rule(relative_path: str, rules: list[dict]) -> dict | None:
    for rule in rules:
        if fnmatch.fnmatch(relative_path, rule["pattern"]):
            return rule
    return None


def write_if_smaller(derived_path: Path, original_bytes: bytes, optimized_bytes: bytes) -> bool:
    if optimized_bytes == original_bytes:
        return False
    derived_path.parent.mkdir(parents=True, exist_ok=True)
    derived_path.write_bytes(optimized_bytes)
    return True


def optimize_loose_images(source_root: Path, derived_root: Path) -> dict[str, int]:
    stats = {"files": 0, "original_bytes": 0, "optimized_bytes": 0}
    for rule in LOOSE_IMAGE_RULES:
        for source_path in sorted(source_root.glob(rule["pattern"])):
            if not source_path.is_file():
                continue
            original_bytes = source_path.read_bytes()
            optimized_bytes = optimize_image_bytes(
                original_bytes,
                source_path.suffix,
                rule["max_dim"],
                rule.get("quality"),
            )
            derived_path = derived_root / source_path.relative_to(source_root)
            if write_if_smaller(derived_path, original_bytes, optimized_bytes):
                stats["files"] += 1
                stats["original_bytes"] += len(original_bytes)
                stats["optimized_bytes"] += len(optimized_bytes)
    return stats


def read_pack_entries(pack_path: Path) -> list[tuple[str, bytes]]:
    with pack_path.open("rb") as pack_file:
        version = struct.unpack(">i", pack_file.read(4))[0]
        if version != 0:
            raise ValueError(f"Unsupported pack format version {version} in {pack_path}")
        entry_count = struct.unpack(">i", pack_file.read(4))[0]
        headers: list[tuple[str, int, int]] = []
        for _ in range(entry_count):
            name_length = struct.unpack(">B", pack_file.read(1))[0]
            name = pack_file.read(name_length).decode("ascii")
            offset, size = struct.unpack(">ii", pack_file.read(8))
            headers.append((name, offset, size))

        entries = []
        for name, offset, size in headers:
            pack_file.seek(offset)
            entries.append((name, pack_file.read(size)))
    return entries


def write_pack_entries(pack_path: Path, entries: list[tuple[str, bytes]]) -> None:
    pack_path.parent.mkdir(parents=True, exist_ok=True)
    with pack_path.open("wb") as pack_file:
        pack_file.write(struct.pack(">i", 0))
        pack_file.write(struct.pack(">i", len(entries)))
        offset = 8 + sum(1 + len(name.encode("ascii")) + 8 for name, _ in entries)
        for name, data in entries:
            name_bytes = name.encode("ascii")
            if len(name_bytes) > 255:
                raise ValueError(f"Pack entry name too long: {name}")
            pack_file.write(struct.pack(">B", len(name_bytes)))
            pack_file.write(name_bytes)
            pack_file.write(struct.pack(">ii", offset, len(data)))
            offset += len(data)
        for _, data in entries:
            pack_file.write(data)


def optimize_pack_files(source_root: Path, derived_root: Path) -> dict[str, dict[str, int]]:
    results: dict[str, dict[str, int]] = {}
    for pack_name, rules in PACK_IMAGE_RULES.items():
        source_path = source_root / "packs" / pack_name
        entries = read_pack_entries(source_path)
        changed = False
        stats = {"files": 0, "original_bytes": 0, "optimized_bytes": 0}
        optimized_entries: list[tuple[str, bytes]] = []
        for entry_name, entry_data in entries:
            rule = match_rule(entry_name, rules)
            if rule and Path(entry_name).suffix.lower() in {".png", ".jpg", ".jpeg"}:
                optimized_bytes = optimize_image_bytes(
                    entry_data,
                    Path(entry_name).suffix,
                    rule["max_dim"],
                    rule.get("quality"),
                )
                if optimized_bytes != entry_data:
                    changed = True
                    stats["files"] += 1
                    stats["original_bytes"] += len(entry_data)
                    stats["optimized_bytes"] += len(optimized_bytes)
                    entry_data = optimized_bytes
            optimized_entries.append((entry_name, entry_data))

        if changed:
            derived_path = derived_root / "packs" / pack_name
            write_pack_entries(derived_path, optimized_entries)
        results[pack_name] = stats
    return results


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    source_root = Path(args.source).resolve()
    derived_root = Path(args.output).resolve()

    if derived_root.exists():
        shutil.rmtree(derived_root)
    derived_root.mkdir(parents=True, exist_ok=True)

    report = {
        "loose_resources": optimize_loose_images(source_root, derived_root),
        "packs": optimize_pack_files(source_root, derived_root),
    }
    (derived_root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
