"""Native transition conversion and production dialog checks with synthetic LODs."""
import copy
import csv
import json
import os
from pathlib import Path
import stat
import subprocess

import pytest

from test_menu_resources import lod, wrapped
from test_story_resources import text, write_archive

TOOL = Path(os.environ.get("TRANSITION_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/transition_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build transition_import first")
HEADER = ["2D#", "Transition Description", "Note"]


def base_rows():
    return [HEADER, ["8", 'Enter the "cave"?\nIt is dark.', "Dragon Cave (area 12)"],
            ["673", "New tunnel description", "Tunnels to Eeofol"], ["1593", "Placeholder Text", ""]]


def source(tmp_path, rows=None, localized=None, version="MMVIII"):
    root = tmp_path / "source"
    data = root / "Data"
    data.mkdir(parents=True)
    write_archive(data / "mmmerge.T.lod", {"trans.txt": base_rows() if rows is None else rows}, version=version)
    if localized is not None:
        write_archive(data / "10 LocZHCN.EnglishT.lod", {"trans.txt": localized}, True, version)
    return root


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True, umask=0o077)


@pytest.mark.parametrize("version", ["MMVI", "MMVIII"])
def test_chinese_old_placeholders_notes_and_ids(tmp_path, version):
    localized = copy.deepcopy(base_rows())
    localized[1][1] = '进入"\x0e \x0e洞\x07 \x0e穴\x07\x0f"？\n这里很黑。'
    localized[2][1:] = ["Placeholder Text", ""]
    localized.append(["60000", "旧版多余文本", "Old entry"])
    root = source(tmp_path, localized=localized, version=version)
    original = {p.name: p.read_bytes() for p in (root / "Data").iterdir()}
    output = tmp_path / "converted"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    with (output / "engine/data_tables/english/trans.txt").open() as stream:
        rows = list(csv.reader(stream, delimiter="\t"))
    assert rows == [HEADER, ["8", '进入"洞穴"？\n这里很黑。', "Dragon Cave (area 12)"],
                    ["673", "New tunnel description", "Tunnels to Eeofol"]]
    report = json.loads((output / "transition_conversion.json").read_text())
    assert report["active_transitions"] == report["dialog_content_checks"] == 2
    assert report["translated"] == 1 and report["restored_chinese_glyphs"] == 2
    assert report["inactive_ids"] == [1593] and report["untranslated_ids"] == [673]
    assert report["preserved_note_differences"] == report["ignored_extra_translations"] == 1
    assert report["typed_validation"] is True and report["runtime_ready"] is False
    assert report["map_transfer_validated"] is report["rendered_dialog_validated"] is False
    assert {p.name: p.read_bytes() for p in (root / "Data").iterdir()} == original
    assert all(stat.S_IMODE(p.stat().st_mode) == 0o644 for p in output.rglob("*") if p.is_file())


def test_english_without_translation_and_crlf(tmp_path):
    root = source(tmp_path)
    write_archive(root / "Data/mmmerge.T.lod", {"trans.txt": base_rows()},
                  raw_texts={"trans.txt": text(base_rows()).replace("\n", "\r\n")})
    output = tmp_path / "converted"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / "transition_conversion.json").read_text())
    assert report["untranslated_ids"] == [8, 673] and report["translated"] == 0


@pytest.mark.parametrize("fault", ["header", "width", "duplicate", "zero", "negative", "suffix", "range",
                                    "empty", "inactive", "identity", "activate"])
def test_invalid_schema_or_identity_has_no_output(tmp_path, fault):
    rows = copy.deepcopy(base_rows())
    localized = None
    if fault == "header":
        rows[0][2] = "Title"
    elif fault == "width":
        rows[1].append("extra")
    elif fault == "duplicate":
        rows.append(rows[1])
    elif fault in ("zero", "negative", "suffix", "range"):
        rows[1][0] = {"zero": "0", "negative": "-1", "suffix": "8abc", "range": "65536"}[fault]
    elif fault == "empty":
        rows = [HEADER]
    elif fault == "inactive":
        rows = [HEADER, ["8", "Placeholder Text", ""]]
    else:
        localized = copy.deepcopy(rows)
        if fault == "identity":
            localized[1][2] = "Wrong destination"
        else:
            localized[3][1] = "Incorrectly activated"
    root = source(tmp_path, rows, localized)
    output = tmp_path / "converted"
    assert run(root, output).returncode != 0
    assert not output.exists()


@pytest.mark.parametrize("fault", ["quotes", "glyphs", "wrapper", "encoding", "missing"])
def test_bad_localized_archive_has_no_output(tmp_path, fault):
    root = source(tmp_path)
    raw = text(base_rows()).encode("gb2312")
    if fault == "quotes":
        raw += b'"unfinished'
    elif fault == "glyphs":
        raw += b"\x0e unfinished"
    elif fault == "encoding":
        raw += b"\xff"
    entry = wrapped("trans.txt", raw) + (b"extra" if fault == "wrapper" else b"")
    lod(root / "Data/10 LocZHCN.EnglishT.lod", {} if fault == "missing" else {"trans.txt": entry})
    output = tmp_path / "converted"
    assert run(root, output).returncode != 0
    assert not output.exists()


def test_existing_nested_and_symlink_paths_preserve_data(tmp_path):
    root = source(tmp_path)
    output = tmp_path / "existing"
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(root, output).returncode != 0
    assert (output / "keep").read_text() == "unchanged"
    assert run(root, root / "converted").returncode != 0
    alias = tmp_path / "alias"
    alias.symlink_to(root, target_is_directory=True)
    assert run(root, alias / "converted").returncode != 0
    archive = root / "Data/mmmerge.T.lod"
    outside = tmp_path / "outside.lod"
    archive.rename(outside)
    archive.symlink_to(outside)
    assert run(root, tmp_path / "converted").returncode != 0
    assert not (tmp_path / "converted").exists()
