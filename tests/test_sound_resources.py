"""Owned executable profile with synthetic sound/voice tables; never executes the EXE."""
import csv
from io import StringIO
import json
import os
from pathlib import Path
import subprocess

import pytest

BUILD = Path(__file__).parents[1] / "build-apple"
TOOL = Path(os.environ.get("SOUND_IMPORT_TOOL", BUILD / "sound_import"))
EXE = Path(os.environ.get("MM8_EXE_FIXTURE", "/Users/yukarii/Downloads/MM678_Merged_05.11.2023_gdrv/mm8.exe"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build sound_import first")


def write(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\r\n".join("\t".join(row) for row in rows) + "\r\n")


@pytest.fixture
def prepared(tmp_path):
    if not EXE.is_file():
        pytest.skip("Supply an owned mm8.exe via MM8_EXE_FIXTURE")
    root, base, output = (tmp_path / name for name in ("source", "base", "output"))
    root.mkdir()
    (root / "mm8.exe").write_bytes(EXE.read_bytes())
    write(root / "Data/Tables/Sounds.txt", [["Name", "Id", "Type", "Is3D"], ["null", "0", "system", "-"],
          ["unused", "0", "0", "-"], ["fountain", "10", "0", "x"], ["first shop", "125", "swap", "-"],
          ["second shop", "125", "lock", "x"], ["persistent", "200", "lock", "-"],
          ['quoted "sound"', "300", "system", "-"]])
    write(base / "engine/data_tables/character_voices.txt", [["Soundtype/Voicesetid", "0", "1"],
          ["Disarmed", "10", "125"], ["Disarmed", "0", "999"]])
    return root, base, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def test_original_sound_fields_first_lookup_and_missing_voice_report(prepared):
    root, base, output = prepared
    before = {p: p.read_bytes() for folder in (root, base) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    expected = [row.split("\t") for row in (root / "Data/Tables/Sounds.txt").read_text().splitlines()]
    rows = list(csv.reader(StringIO((output / "engine/data_tables/sounds.txt").read_text()), delimiter="\t"))
    assert rows == [["#SoundCatalogMode", "MM8OrderedV1"], *expected]
    report = json.loads((output / "sound_conversion.json").read_text())
    assert report["source_rows"] == 7 and report["lookup_ids"] == 5 and report["zero_rows"] == 2
    assert report["positional_rows"] == 2 and report["voice_references"] == 3
    assert report["missing_voice_ids"] == [999] and report["audio_files_exported"] == 0
    assert report["duplicates"] == [{"id": 0, "source_row": 2, "selected": "null", "shadowed": "unused"},
                                    {"id": 125, "source_row": 5, "selected": "first shop", "shadowed": "second shop"}]
    assert report["typed_validation"] and not report["runtime_ready"]
    assert not report["audio_playback_validated"] and not report["executable_executed"]
    assert not report["original_cache_policy_applied"] and not report["patched_windows_parity"]
    assert all(p.read_bytes() == value for p, value in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["header", "width", "id", "negative_id", "large_id", "type", "position",
                                    "sentinel", "name", "fingerprint", "voices"])
def test_invalid_sound_source_does_not_publish(prepared, fault):
    root, base, output = prepared
    path = root / "Data/Tables/Sounds.txt"
    rows = [line.split("\t") for line in path.read_text().splitlines()]
    if fault == "header":
        rows[0][3] = "Different"
    elif fault == "width":
        rows[3].append("unexpected")
    elif fault in ("id", "negative_id", "large_id"):
        rows[3][1] = {"id": "ten", "negative_id": "-1", "large_id": "4294967296"}[fault]
    elif fault == "type":
        rows[3][2] = "unknown"
    elif fault == "position":
        rows[3][3] = "3d"
    elif fault == "sentinel":
        rows[1][0] = "other"
    elif fault == "name":
        rows[3][0] = ""
    elif fault == "fingerprint":
        (root / "mm8.exe").write_bytes(b"unsupported executable")
    else:
        (base / "engine/data_tables/character_voices.txt").unlink()
    write(path, rows)
    assert run(prepared).returncode != 0 and not output.exists()


@pytest.mark.parametrize("input_file", ["mm8.exe", "Data/Tables/Sounds.txt"])
def test_input_symlinks_do_not_escape_root(prepared, tmp_path, input_file):
    path = prepared[0] / input_file
    outside = tmp_path / "outside"
    path.rename(outside)
    path.symlink_to(outside)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_destination_boundaries_and_existing_files(prepared):
    root, base, output = prepared
    for parent in (root, base):
        assert run((root, base, parent / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"


def test_shared_sound_catalog_contracts():
    tool = BUILD / "sound_catalog_check"
    assert tool.is_file(), "Build sound_catalog_check"
    result = subprocess.run([str(tool)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
