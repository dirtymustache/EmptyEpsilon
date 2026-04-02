#!/usr/bin/env python3
"""Stage a browser-friendly EmptyEpsilon asset set for Emscripten builds."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


MINIMAL_RESOURCE_EXCLUDES = {
    "audio",
    "music",
}

BRIDGE_RESOURCE_INCLUDE_DIRS = {
    "cursors",
    "gui",
    "locale",
    "radar",
    "sfx",
    "shaders",
}

BRIDGE_RESOURCE_INCLUDE_FILES = {
    "gradient.png",
    "logo_full.png",
    "logo_icon.png",
    "logo_white.png",
    "noise.png",
    "redicule.png",
    "waypoint.png",
}


def copy_tree(source: Path, target: Path, ignore=None) -> None:
    if not source.exists():
        return
    shutil.copytree(source, target, dirs_exist_ok=True, ignore=ignore)


def stage_resources(source_root: Path, output_root: Path, profile: str) -> None:
    resource_source = source_root / "resources"
    resource_target = output_root / "resources"
    resource_target.mkdir(parents=True, exist_ok=True)

    if profile == "bridge":
        for entry in resource_source.iterdir():
            if entry.is_dir() and entry.name in BRIDGE_RESOURCE_INCLUDE_DIRS:
                copy_tree(entry, resource_target / entry.name)
            elif entry.is_file() and entry.name in BRIDGE_RESOURCE_INCLUDE_FILES:
                shutil.copy2(entry, resource_target / entry.name)
    elif profile == "minimal":
        copy_tree(
            resource_source,
            resource_target,
            ignore=shutil.ignore_patterns(*MINIMAL_RESOURCE_EXCLUDES),
        )
    else:
        copy_tree(resource_source, resource_target)


def stage_scripts(source_root: Path, output_root: Path) -> None:
    copy_tree(source_root / "scripts", output_root / "scripts")


def stage_packs(source_root: Path, output_root: Path, profile: str) -> None:
    packs_target = output_root / "packs"
    packs_target.mkdir(parents=True, exist_ok=True)
    if profile == "full":
        copy_tree(source_root / "packs", packs_target)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="Source repository root")
    parser.add_argument("--output", required=True, help="Output staging directory")
    parser.add_argument("--profile", choices=("bridge", "minimal", "full"), default="minimal")
    args = parser.parse_args()

    source_root = Path(args.source).resolve()
    output_root = Path(args.output).resolve()

    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    stage_resources(source_root, output_root, args.profile)
    if args.profile != "bridge":
        stage_scripts(source_root, output_root)
    stage_packs(source_root, output_root, args.profile)

    marker = output_root / f".profile-{args.profile}"
    marker.write_text(args.profile + "\n", encoding="ascii")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
