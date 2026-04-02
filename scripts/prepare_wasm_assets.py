#!/usr/bin/env python3
"""Stage browser preload assets and build explicit browser asset bundles."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
from pathlib import Path

ASSET_MANIFEST_DIR = "asset_manifests"
BUNDLE_MAGIC = b"EEBNDL1\x00"
BUNDLE_VERSION = 1
def copy_tree(source: Path, target: Path, ignore=None) -> None:
    if not source.exists():
        return
    shutil.copytree(source, target, dirs_exist_ok=True, ignore=ignore)


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def load_bundle_definition(source_root: Path, bundle_id: str) -> dict:
    return load_json(source_root / ASSET_MANIFEST_DIR / "bundles" / f"{bundle_id}.json")


def collect_files(path: Path) -> list[Path]:
    if path.is_file():
        return [path]
    return sorted(child for child in path.rglob("*") if child.is_file())


def resolve_source_path(source_root: Path, derived_root: Path | None, relative_path: str) -> Path:
    if derived_root is not None:
        candidate = derived_root / relative_path
        if candidate.exists():
            return candidate
    return source_root / relative_path


def stage_bundle_entries(
    source_root: Path,
    derived_root: Path | None,
    output_root: Path,
    entries: list[dict],
    resource_only: bool = False,
) -> None:
    for entry in entries:
        source = resolve_source_path(source_root, derived_root, entry["source"])
        if not source.exists():
            raise FileNotFoundError(f"Missing bundle source: {source}")
        if resource_only and not entry["target"].startswith("resources/"):
            continue
        target = output_root / entry["target"]
        if source.is_dir():
            copy_tree(source, target)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)


def stage_resources(source_root: Path, derived_root: Path | None, output_root: Path, profile: str) -> None:
    resource_target = output_root / "resources"
    resource_target.mkdir(parents=True, exist_ok=True)

    if profile in {"bridge", "minimal"}:
        bundle_manifest = load_bundle_definition(source_root, "core_browser_shell")
        stage_bundle_entries(source_root, derived_root, output_root if profile == "minimal" else resource_target.parent, bundle_manifest["include"], resource_only=(profile == "bridge"))
    else:
        copy_tree(source_root / "resources", resource_target)
        if derived_root and (derived_root / "resources").exists():
            copy_tree(derived_root / "resources", resource_target)


def stage_scripts(source_root: Path, output_root: Path) -> None:
    copy_tree(source_root / "scripts", output_root / "scripts")


def stage_packs(source_root: Path, derived_root: Path | None, output_root: Path, profile: str) -> None:
    packs_target = output_root / "packs"
    packs_target.mkdir(parents=True, exist_ok=True)
    if profile == "full":
        copy_tree(source_root / "packs", packs_target)
        if derived_root and (derived_root / "packs").exists():
            copy_tree(derived_root / "packs", packs_target)


def write_bundle_payload(artifact_path: Path, payload_files: list[dict]) -> None:
    with artifact_path.open("wb") as artifact:
        artifact.write(BUNDLE_MAGIC)
        artifact.write(struct.pack("<II", BUNDLE_VERSION, len(payload_files)))
        for file_entry in payload_files:
            path_bytes = file_entry["path"].encode("utf-8")
            data_bytes = file_entry["data"]
            artifact.write(struct.pack("<II", len(path_bytes), len(data_bytes)))
            artifact.write(path_bytes)
            artifact.write(data_bytes)


def build_generated_bundles(source_root: Path, derived_root: Path | None, output_root: Path) -> None:
    manifests_root = source_root / ASSET_MANIFEST_DIR
    bundle_defs_root = manifests_root / "bundles"
    scenario_defs_root = manifests_root / "scenarios"
    generated_root = output_root / "asset_bundles"
    if output_root.name == "wasm_assets":
        generated_root = output_root.parent / "asset_bundles"
    if generated_root.exists():
        shutil.rmtree(generated_root)
    bundle_manifest_root = generated_root / "manifests"
    bundle_payload_root = generated_root / "bundles"
    scenario_manifest_root = generated_root / "scenarios"
    bundle_manifest_root.mkdir(parents=True, exist_ok=True)
    bundle_payload_root.mkdir(parents=True, exist_ok=True)
    scenario_manifest_root.mkdir(parents=True, exist_ok=True)

    bundle_runtime_manifests: dict[str, dict] = {}

    for bundle_path in sorted(bundle_defs_root.glob("*.json")):
        bundle_definition = load_json(bundle_path)
        payload_files = []
        digest = hashlib.sha256()
        total_size = 0
        for entry in bundle_definition.get("include", []):
            source = resolve_source_path(source_root, derived_root, entry["source"])
            if not source.exists():
                raise FileNotFoundError(f"Missing bundle source: {source}")
            target_root = Path(entry["target"])
            if source.is_dir():
                for file_path in collect_files(source):
                    relative_path = file_path.relative_to(source)
                    runtime_path = (target_root / relative_path).as_posix()
                    file_bytes = file_path.read_bytes()
                    digest.update(runtime_path.encode("utf-8"))
                    digest.update(b"\0")
                    digest.update(file_bytes)
                    total_size += len(file_bytes)
                    payload_files.append(
                        {
                            "path": runtime_path,
                            "size": len(file_bytes),
                            "sha256": hashlib.sha256(file_bytes).hexdigest(),
                            "data": file_bytes,
                        }
                    )
            else:
                runtime_path = target_root.as_posix()
                file_bytes = source.read_bytes()
                digest.update(runtime_path.encode("utf-8"))
                digest.update(b"\0")
                digest.update(file_bytes)
                total_size += len(file_bytes)
                payload_files.append(
                    {
                        "path": runtime_path,
                        "size": len(file_bytes),
                        "sha256": hashlib.sha256(file_bytes).hexdigest(),
                        "data": file_bytes,
                    }
                )

        content_hash = digest.hexdigest()[:16]
        artifact_filename = f"{bundle_definition['bundle_id']}-{content_hash}.bundle.bin"
        artifact_path = bundle_payload_root / artifact_filename
        write_bundle_payload(artifact_path, payload_files)

        runtime_manifest = {
            "schema_version": 1,
            "bundle_id": bundle_definition["bundle_id"],
            "content_hash": content_hash,
            "artifact_filename": artifact_filename,
            "artifact_format": "binary_bundle_v1",
            "artifact_url": f"/asset_bundles/bundles/{artifact_filename}",
            "fallback_url": f"/admin-api/browser/bundles/{artifact_filename}",
            "byte_size": total_size,
        }
        (bundle_manifest_root / f"{bundle_definition['bundle_id']}.json").write_text(
            json.dumps(runtime_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        bundle_runtime_manifests[bundle_definition["bundle_id"]] = runtime_manifest

    scenario_index = {"schema_version": 1, "scenarios": {}}
    for scenario_path in sorted(scenario_defs_root.glob("*.json")):
        scenario_definition = load_json(scenario_path)
        required_bundles = [
            bundle_runtime_manifests[bundle_id]
            for bundle_id in scenario_definition.get("required_bundles", [])
        ]
        optional_bundles = [
            bundle_runtime_manifests[bundle_id]
            for bundle_id in scenario_definition.get("optional_bundles", [])
        ]
        generated_manifest = {
            "schema_version": 1,
            "scenario_id": scenario_definition["scenario_id"],
            "scenario_file": scenario_definition["scenario_file"],
            "revision": scenario_definition["revision"],
            "required_bundles": required_bundles,
            "optional_bundles": optional_bundles,
        }
        generated_path = scenario_manifest_root / scenario_path.name
        generated_path.write_text(json.dumps(generated_manifest, indent=2) + "\n", encoding="utf-8")
        scenario_index["scenarios"][scenario_definition["scenario_file"]] = {
            "scenario_manifest_url": f"/asset_bundles/scenarios/{scenario_path.name}",
            "scenario_manifest_path": scenario_path.name,
            "revision": scenario_definition["revision"],
        }

    (generated_root / "index.json").write_text(json.dumps(scenario_index, indent=2) + "\n", encoding="utf-8")


def build_web_derived_assets(source_root: Path, derived_root: Path) -> None:
    helper_script = source_root / "scripts" / "build_web_derived_assets.py"
    candidates: list[list[str]] = []
    configured = os.environ.get("EE_HOST_PYTHON", "").strip()
    if configured:
        candidates.append([configured])
    candidates.append(["py", "-3"])
    candidates.append(["python3"])
    candidates.append(["python"])

    last_error: Exception | None = None
    for command in candidates:
        try:
            subprocess.run(
                command + [str(helper_script), "--source", str(source_root), "--output", str(derived_root)],
                check=True,
            )
            return
        except (FileNotFoundError, subprocess.CalledProcessError) as error:
            last_error = error
    raise RuntimeError(f"Failed to build derived web assets with any host python candidate: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="Source repository root")
    parser.add_argument("--output", required=True, help="Output staging directory")
    parser.add_argument("--profile", choices=("bridge", "minimal", "full"), default="minimal")
    args = parser.parse_args()

    source_root = Path(args.source).resolve()
    output_root = Path(args.output).resolve()

    derived_root = output_root / "web_derived_assets"
    if output_root.name == "wasm_assets":
        derived_root = output_root.parent / "web_derived_assets"

    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    build_web_derived_assets(source_root, derived_root)
    stage_resources(source_root, derived_root, output_root, args.profile)
    if args.profile != "bridge":
        stage_scripts(source_root, output_root)
    stage_packs(source_root, derived_root, output_root, args.profile)
    build_generated_bundles(source_root, derived_root, output_root)

    marker = output_root / f".profile-{args.profile}"
    marker.write_text(args.profile + "\n", encoding="ascii")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
