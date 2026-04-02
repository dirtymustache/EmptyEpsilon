#!/usr/bin/env python3
"""Stage the full EmptyEpsilon asset set for Emscripten builds."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def copy_tree(source: Path, target: Path, ignore=None) -> None:
    if not source.exists():
        return
    shutil.copytree(source, target, dirs_exist_ok=True, ignore=ignore)


def stage_resources(source_root: Path, output_root: Path) -> None:
    resource_source = source_root / "resources"
    resource_target = output_root / "resources"
    resource_target.mkdir(parents=True, exist_ok=True)
    copy_tree(resource_source, resource_target)


def stage_scripts(source_root: Path, output_root: Path) -> None:
    copy_tree(source_root / "scripts", output_root / "scripts")


def stage_packs(source_root: Path, output_root: Path) -> None:
    packs_target = output_root / "packs"
    packs_target.mkdir(parents=True, exist_ok=True)
    copy_tree(source_root / "packs", packs_target)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="Source repository root")
    parser.add_argument("--output", required=True, help="Output staging directory")
    args = parser.parse_args()

    source_root = Path(args.source).resolve()
    output_root = Path(args.output).resolve()

    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    stage_resources(source_root, output_root)
    stage_scripts(source_root, output_root)
    stage_packs(source_root, output_root)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
