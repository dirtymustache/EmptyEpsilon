#!/usr/bin/env python3
"""Regenerate checked-in browser scenario manifests from scenario usage heuristics."""

from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path


PACK_BUNDLE_IDS = {
    "Angryfly.pack": "pack_angryfly",
    "Asteroids.pack": "pack_asteroids",
    "msgamedev.pack": "pack_msgamedev",
}

SHARED_BUNDLE_ID = "scenario_resources_common"
SCENARIO_REVISION = 2
AUDIO_BUNDLE_IDS = {
    "34": "scenario_audio_34",
    "48": "scenario_audio_48",
    "51": "scenario_audio_51",
    "54": "scenario_audio_54",
    "55": "scenario_audio_55",
    "58": "scenario_audio_58",
    "62": "scenario_audio_62",
}

STRING_LITERAL_RE = re.compile(r'"([^"\n]+)"')
SET_TEMPLATE_LITERAL_RE = re.compile(r'setTemplate\("([^"]+)"\)')
SET_MODEL_LITERAL_RE = re.compile(r'setModel\("([^"]+)"\)')
SET_NAME_LITERAL_RE = re.compile(r'setName\("([^"]+)"\)')
COPY_LITERAL_RE = re.compile(r':copy\("([^"]+)"\)')
ASSIGN_VAR_RE = re.compile(r'^\s*(?:local\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=')
METHOD_VAR_RE = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:')
SCENARIO_NUMBER_RE = re.compile(r"scenario_(\d+)_")


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def iter_pack_entries(pack_path: Path) -> list[str]:
    with pack_path.open("rb") as pack_file:
        _version = struct.unpack(">i", pack_file.read(4))[0]
        count = struct.unpack(">i", pack_file.read(4))[0]
        names = []
        for _ in range(count):
            name_length = struct.unpack(">B", pack_file.read(1))[0]
            name = pack_file.read(name_length).decode("ascii")
            pack_file.read(8)
            names.append(name)
    return names


def build_model_pack_index(source_root: Path) -> dict[str, str]:
    model_to_pack: dict[str, str] = {}
    for pack_name, bundle_id in PACK_BUNDLE_IDS.items():
        for entry_name in iter_pack_entries(source_root / "packs" / pack_name):
            root = entry_name.split("/", 1)[0]
            stem = Path(root).stem
            if stem:
                model_to_pack.setdefault(stem, bundle_id)
                model_to_pack.setdefault(root, bundle_id)
    return model_to_pack


def resolve_pack_for_model(model_name: str, model_to_pack: dict[str, str]) -> str | None:
    if model_name in model_to_pack:
        return model_to_pack[model_name]

    best_prefix = None
    for prefix, bundle_id in model_to_pack.items():
        if model_name.startswith(prefix):
            if best_prefix is None or len(prefix) > len(best_prefix[0]):
                best_prefix = (prefix, bundle_id)
    if best_prefix is not None:
        return best_prefix[1]
    return None


def build_template_pack_index(source_root: Path, model_to_pack: dict[str, str]) -> tuple[dict[str, set[str]], set[str]]:
    template_to_packs: dict[str, set[str]] = {}
    known_templates: set[str] = set()

    for path in sorted((source_root / "scripts" / "shiptemplates").glob("*.lua")):
        variables: dict[str, set[str]] = {}
        for raw_line in path.read_text(encoding="utf-8").splitlines():
            line = raw_line.split("--", 1)[0]
            if not line.strip():
                continue

            assigned_var_match = ASSIGN_VAR_RE.match(line)
            method_var_match = METHOD_VAR_RE.match(line)
            current_var = assigned_var_match.group(1) if assigned_var_match else None
            if current_var is None and method_var_match:
                current_var = method_var_match.group(1)

            template_names = set(SET_NAME_LITERAL_RE.findall(line))
            template_names.update(COPY_LITERAL_RE.findall(line))
            if current_var and template_names:
                variables[current_var] = template_names
            elif current_var and current_var not in variables and "ShipTemplate()" in line:
                variables[current_var] = set()

            active_templates = set(template_names)
            if current_var:
                active_templates.update(variables.get(current_var, set()))
            if not active_templates:
                continue

            known_templates.update(active_templates)
            packs = {
                resolve_pack_for_model(model_name, model_to_pack)
                for model_name in SET_MODEL_LITERAL_RE.findall(line)
            }
            packs.discard(None)
            if not packs:
                continue

            for template_name in active_templates:
                template_to_packs.setdefault(template_name, set()).update(packs)

    return template_to_packs, known_templates


def determine_required_bundles(
    scenario_path: Path,
    template_to_packs: dict[str, set[str]],
    known_templates: set[str],
    model_to_pack: dict[str, str],
) -> list[str]:
    text = scenario_path.read_text(encoding="utf-8")
    required = {SHARED_BUNDLE_ID}

    if "Asteroid()" in text or "VisualAsteroid()" in text:
        required.add("pack_asteroids")

    for template_name in SET_TEMPLATE_LITERAL_RE.findall(text):
        required.update(template_to_packs.get(template_name, ()))

    for model_name in SET_MODEL_LITERAL_RE.findall(text):
        bundle_id = resolve_pack_for_model(model_name, model_to_pack)
        if bundle_id:
            required.add(bundle_id)

    for literal in STRING_LITERAL_RE.findall(text):
        if literal in known_templates:
            required.update(template_to_packs.get(literal, ()))
            continue
        bundle_id = resolve_pack_for_model(literal, model_to_pack)
        if bundle_id:
            required.add(bundle_id)

    # Conservative fallback for scenarios that create ships but only via dynamic/template-table code.
    if ("CpuShip()" in text or "PlayerSpaceship()" in text or "SpaceStation()" in text) and len(required) == 1:
        required.update(("pack_angryfly", "pack_msgamedev"))

    number_match = SCENARIO_NUMBER_RE.search(scenario_path.name)
    if number_match:
        audio_bundle_id = AUDIO_BUNDLE_IDS.get(number_match.group(1))
        if audio_bundle_id:
            required.add(audio_bundle_id)

    bundle_order = [
        SHARED_BUNDLE_ID,
        "pack_angryfly",
        "pack_asteroids",
        "pack_msgamedev",
        "scenario_audio_34",
        "scenario_audio_48",
        "scenario_audio_51",
        "scenario_audio_54",
        "scenario_audio_55",
        "scenario_audio_58",
        "scenario_audio_62",
    ]
    return [bundle_id for bundle_id in bundle_order if bundle_id in required]


def update_scenario_manifest(manifest_path: Path, required_bundles: list[str]) -> None:
    manifest = load_json(manifest_path)
    manifest["revision"] = SCENARIO_REVISION
    manifest["required_bundles"] = required_bundles
    manifest["optional_bundles"] = []
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="Source repository root")
    args = parser.parse_args()

    source_root = Path(args.source).resolve()
    model_to_pack = build_model_pack_index(source_root)
    template_to_packs, known_templates = build_template_pack_index(source_root, model_to_pack)
    scenarios_root = source_root / "asset_manifests" / "scenarios"
    scripts_root = source_root / "scripts"

    summary: dict[str, list[str]] = {}
    for manifest_path in sorted(scenarios_root.glob("*.json")):
        manifest = load_json(manifest_path)
        scenario_file = manifest["scenario_file"]
        required_bundles = determine_required_bundles(
            scripts_root / scenario_file,
            template_to_packs,
            known_templates,
            model_to_pack,
        )
        update_scenario_manifest(manifest_path, required_bundles)
        summary[scenario_file] = required_bundles

    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
