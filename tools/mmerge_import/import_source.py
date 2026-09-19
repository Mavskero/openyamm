"""Audit and stage user-owned MMMerge sources. Output is NOT yet a runtime pack.

Each archive stays in its own namespace. This intentionally avoids guessing the
Windows mod loader's override order or replacing OpenYAMM's canonical tables.
"""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile

from chinese_text import decode_legacy_text, remaining_controls
from legacy_lod import ImportFormatError, LodArchive


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def contained_output(source: Path, output: Path) -> bool:
    return output == source or output.is_relative_to(source) or source.is_relative_to(output)


def stage_source(source: Path, output: Path, extract_maps: bool = False) -> dict:
    source, output = source.resolve(), output.resolve()
    if not source.is_dir() or not (source / "Data").is_dir():
        raise ValueError("Select a complete installed MMMerge directory containing Data/")
    if contained_output(source, output):
        raise ValueError("Source and output must be separate, non-overlapping directories")
    if output.exists():
        raise ValueError("Output already exists; choose a new path to preserve the previous import")
    config = configparser.ConfigParser(interpolation=None)
    config_path = source / "Data/LocalizeConf.ini"
    if config_path.exists():
        config.read_string(config_path.read_bytes().decode("gb18030", errors="strict"))
    encoding = config.get("Settings", "encoding", fallback="gb2312")
    language = config.get("Settings", "lang", fallback="unknown")
    report: dict = {
        "schema": 1, "stage": "source-audit", "runtime_ready": False,
        "source": str(source), "language": language, "declared_encoding": encoding,
        "localization_target_merge": config.get("Settings", "merge_version", fallback=None),
        "archives": [], "loose_texts": [], "errors": [], "warnings": [],
        "runtime_blockers": [
            "Asset conversion and archive override mapping are not complete.",
            "Translated text must be mapped to OpenYAMM canonical table/event IDs.",
            "Windows EXE/DLL/Lua memory patches are not executable in OpenYAMM.",
        ],
    }
    required = [f"Data/mm{version}.{kind}.lod" for version in (6, 7, 8)
                for kind in ("games", "icons", "bitmaps")]
    required += ["Data/mmmerge.T.lod", "Data/mmmerge.D.lod"]
    for relative in required:
        if not (source / relative).is_file():
            report["errors"].append({"path": relative, "error": "Missing required source archive"})
    report["missing_mm8_base_archives"] = [f"Data/{name}.lod" for name in ("bitmaps", "sprites", "icons")
                                            if not (source / f"Data/{name}.lod").is_file()]
    if report["missing_mm8_base_archives"]:
        report["warnings"].append("MM8 base archives are missing; the mod files alone are not a complete installation: "
                                  + ", ".join(report["missing_mm8_base_archives"]))
        report["runtime_blockers"].append("Supply the missing MM8 base resources to validate full merged gameplay.")
    if report["localization_target_merge"]:
        report["warnings"].append(
            "Language configuration targets Merge " + report["localization_target_merge"]
            + "; compatibility with the supplied newer mod needs table-level validation.")
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".mmerge-import-", dir=output.parent))
    try:
        for path in sorted(source.rglob("*")):
            if not path.is_file() or path.suffix.casefold() != ".lod":
                continue
            relative = path.relative_to(source)
            try:
                if not path.resolve().is_relative_to(source):
                    raise ValueError("Source symlink points outside the selected directory")
                archive = LodArchive(path)
                item = {"path": str(relative), "bytes": archive.size, "sha256": digest(path),
                        "format": archive.version, "members": len(archive.entries),
                        "texts": [], "maps": [], "index": [e.name for e in archive.entries]}
                report["archives"].append(item)
                for entry in archive.entries:
                    suffix = Path(entry.name).suffix.casefold()
                    member_path = staging / "archives" / relative / entry.name.lower()
                    try:
                        if suffix in (".txt", ".str"):
                            payload = archive.read_text_payload(entry)
                            localized = "loczhcn" in path.name.casefold()
                            text, glyphs = decode_legacy_text(payload, encoding if localized else "cp1252")
                            member_path.parent.mkdir(parents=True, exist_ok=True)
                            member_path.write_text(text, encoding="utf-8", newline="\n")
                            item["texts"].append({"name": entry.name, "glyphs_restored": glyphs,
                                "characters": len(text), "remaining_controls": remaining_controls(text)})
                        elif extract_maps and suffix in (".blv", ".odm", ".dlv", ".ddm"):
                            payload = archive.read_map_payload(entry)
                            member_path.parent.mkdir(parents=True, exist_ok=True)
                            member_path.write_bytes(payload)
                            item["maps"].append({"name": entry.name, "bytes": len(payload)})
                    except (ValueError, UnicodeError) as error:
                        report["errors"].append({"path": f"{relative}/{entry.name}", "error": str(error)})
            except (OSError, ValueError, UnicodeError) as error:
                report["errors"].append({"path": str(relative), "error": str(error)})
        loose_paths = list((source / "Data/Text localization").glob("*.txt"))
        loose_paths += list((source / "Data/Tables").glob("*.txt"))
        for path in sorted(loose_paths):
            relative = path.relative_to(source)
            try:
                if not path.resolve().is_relative_to(source):
                    raise ValueError("Source symlink points outside the selected directory")
                localized = "Text localization" in path.parts
                text, glyphs = decode_legacy_text(path.read_bytes(), encoding if localized else "cp1252")
                destination = staging / "loose" / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text(text, encoding="utf-8", newline="\n")
                report["loose_texts"].append({"path": str(relative), "sha256": digest(path),
                    "glyphs_restored": glyphs, "remaining_controls": remaining_controls(text)})
            except (OSError, ValueError, UnicodeError) as error:
                report["errors"].append({"path": str(relative), "error": str(error)})
        report["audit_ok"] = not report["errors"]
        (staging / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                               encoding="utf-8")
        staging.rename(output)
    except BaseException:
        shutil.rmtree(staging)
        raise
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--extract-maps", action="store_true")
    args = parser.parse_args()
    try:
        report = stage_source(args.source, args.output, args.extract_maps)
    except (OSError, ValueError) as error:
        parser.exit(2, f"Import failed: {error}\n")
    texts = sum(len(a["texts"]) for a in report["archives"])
    maps = sum(len(a["maps"]) for a in report["archives"])
    print(json.dumps({"archives": len(report["archives"]), "texts": texts, "maps": maps,
                      "errors": len(report["errors"]), "manifest": str(args.output / "manifest.json"),
                      "runtime_ready": False}, ensure_ascii=False))
    return 0 if report["audit_ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
