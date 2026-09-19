"""Export scene candidates from an audited MMMerge import, preserving archive namespaces.

Uses OpenYAMM's existing geometry/companion readers. Table IDs, archive overrides
and event integration are not resolved here; the output cannot start gameplay.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import export_indoor_scene_yml as indoor
import export_outdoor_scene_yml as outdoor


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def export_scenes(audit_root: Path, output: Path) -> dict:
    audit_root, output = audit_root.resolve(), output.resolve()
    if output.is_relative_to(audit_root) or audit_root.is_relative_to(output) or output.exists():
        raise ValueError("Output must be new and separate from the audited resources")
    audit = json.loads((audit_root / "manifest.json").read_text())
    if audit.get("stage") != "source-audit" or audit.get("errors"):
        raise ValueError("Use a completed source audit without errors")
    archive_hashes = {entry["path"]: entry["sha256"] for entry in audit["archives"]}
    report = {"schema": 1, "stage": "scene-candidates", "runtime_ready": False,
              "source_audit_sha256": digest(audit_root / "manifest.json"),
              "scenes": [], "errors": [], "runtime_blockers": [
                  "Candidate scenes retain legacy IDs; merged registries and overrides need validation.",
                  "Textures, sprites, audio and Lua events are not included in this export.",
                  "Successful parsing does not establish collision, quests, gameplay or iPad rendering."]}
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".scene-export-", dir=output.parent))
    try:
        for archive in sorted((audit_root / "archives/Data").iterdir()):
            if not archive.name.endswith(".games.lod"):
                continue
            if archive.is_symlink() or not archive.is_dir():
                raise ValueError("Expected an extracted local archive directory")
            world = archive.name.split(".")[0]
            world = world if world in ("mm6", "mm7", "mm8") else "auto"
            for geometry in sorted(archive.iterdir()):
                if geometry.suffix not in (".blv", ".odm"):
                    continue
                is_indoor = geometry.suffix == ".blv"
                companion = geometry.with_suffix(".dlv" if is_indoor else ".ddm")
                try:
                    if geometry.is_symlink() or companion.is_symlink() or not companion.is_file():
                        raise ValueError("Missing or linked geometry/companion")
                    if is_indoor:
                        model = indoor.parse_indoor_blv(geometry)
                        args = argparse.Namespace(world=world)
                        format_id = indoor.infer_dlv_format_id(args, model)
                        state = indoor.parse_indoor_dlv(companion, model, indoor.DLV_FORMATS[format_id])
                        scene = indoor.build_scene_model(model, state, world)
                        text = indoor.render_scene_yaml(scene)
                    else:
                        model = outdoor.parse_outdoor_odm(geometry)
                        state = outdoor.parse_outdoor_ddm(companion, model)
                        scene = outdoor.build_scene_model(model, state)
                        outdoor.validate_scene_model(scene)
                        text = outdoor.render_scene_yaml(scene)
                    relative = Path("archives") / archive.name / f"{geometry.stem}.scene.yml"
                    target = staging / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_text(text, encoding="utf-8")
                    shutil.copyfile(geometry, target.with_suffix("").with_suffix(geometry.suffix))
                    report["scenes"].append({
                        "source_archive": archive.name,
                        "source_archive_sha256": archive_hashes[f"Data/{archive.name}"],
                        "geometry": geometry.name, "geometry_sha256": digest(geometry),
                        "companion_sha256": digest(companion), "output": relative.as_posix(),
                        "source_world": world, "geometry_version": model["version"],
                        "kind": scene["kind"], "entities": len(scene.get("entities", [])),
                        "spawns": len(scene.get("spawns", [])), "scene_sha256": digest(target)})
                except (ValueError, RuntimeError, OSError, KeyError) as error:
                    report["errors"].append({"archive": archive.name, "geometry": geometry.name,
                                             "error": str(error)})
        if not report["scenes"] and not report["errors"]:
            raise ValueError("No extracted map pairs; audit with --extract-maps first")
        (staging / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        staging.rename(output)
    except BaseException:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("audit_root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    report = export_scenes(args.audit_root, args.output)
    print(json.dumps({"scenes": len(report["scenes"]), "errors": len(report["errors"]),
                      "runtime_ready": False}))
    return 1 if report["errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
