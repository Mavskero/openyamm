"""Exercise native story conversion using synthetic owned-data layouts."""
import csv
from io import StringIO
import json
import os
from pathlib import Path
import stat
import subprocess

import pytest

from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("STORY_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/story_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build story_import first")


def text(rows):
    out = StringIO()
    csv.writer(out, delimiter="\t", lineterminator="\n").writerows(rows)
    return out.getvalue()


def fixtures():
    return {
        "npcdata.txt": [["1", "Resident", "50", "0", "0", "0", "30", "9", "1", "0",
                         "601", "0", "0", "0", "0", "0"]],
        "npcgreet.txt": [["1", "Welcome\ntraveller", "Welcome back", "", "Resident"]],
        "npctext.txt": [["200", 'Here is a "quest".\nRead it.', "", "Owner"], ["201", "Party is full"]],
        "npctopic.txt": [["601", "Roster Join Event", "0", "", "200. Resident", "Resident", ""]],
        "npcnews.txt": [["1", "News body", "News title"]],
        "npcgroup.txt": [["1", "1"]],
        "quests.txt": [["10", "Deliver the letter", "Keep mechanic notes", "Owner"]],
    }


def write_archive(path, rows, chinese=False, version="MMVIII", fault=None, raw_texts=None):
    entries = {}
    for name, table in rows.items():
        data = (raw_texts or {}).get(name, text(table)).encode("gb2312" if chinese else "cp1252")
        if name == "quests.txt":
            if fault == "encoding":
                data += b"\xff"
            elif fault == "quotes":
                data += b'"unterminated'
            elif fault == "glyphs":
                data += b"\x0e unfinished"
        entries[name] = wrapped(name, data, name_size=64 if version == "MMVIII" else 16)
    if fault == "wrapper":
        entries["quests.txt"] += b"extra"
    lod(path, entries, version)


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True, umask=0o077)


def read_rows(path):
    with path.open() as stream:
        return {r[0]: r for r in csv.reader(stream, delimiter="\t") if r and r[0].isdigit()}


@pytest.mark.parametrize("version", ["MMVI", "MMVIII"])
def test_unicode_merge_preserves_mechanics_multiline_and_source(tmp_path, version):
    root = tmp_path / "source"
    data = root / "Data"
    data.mkdir(parents=True)
    base = fixtures()
    localized = fixtures()
    localized.pop("npcgroup.txt")
    localized["npcdata.txt"][0][1] = "居民"
    localized["npcdata.txt"][0][6] = "999"  # Old translation must not move the NPC.
    localized["npctopic.txt"][0][1] = "加入"
    localized["npctopic.txt"][0][4] = "999. Old link"
    localized["npctext.txt"][0][1] = '他说："你好"\n\x0e \x0e任\x07 \x0e务\x07\x0f'
    localized["quests.txt"][0][1] = "递送信件"
    localized["quests.txt"].append(["11", "obsolete translation", "", ""])
    base["quests.txt"].append(["12", "New quest", "", ""])
    write_archive(data / "mmmerge.T.lod", base, version=version)
    write_archive(data / "10 LocZHCN.EnglishT.lod", localized, True, version)
    original = {p.name: p.read_bytes() for p in data.iterdir()}
    output = tmp_path / "story"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    tables = output / "engine/data_tables"
    npc = read_rows(tables / "npc.txt")["1"]
    assert npc[1] == "居民" and npc[6] == "30"
    topic = read_rows(tables / "npc_topic.txt")["601"]
    assert topic[1] == "加入" and topic[4] == "200. Resident" and topic[7] == "Roster Join Event"
    assert read_rows(tables / "npc_topic_text.txt")["200"][1] == '他说："你好"\n任务'
    quests = read_rows(tables / "english/quests.txt")
    assert set(quests) == {"10", "12"} and quests["12"][1] == "New quest"
    assert quests["10"][1:] == ["递送信件", "Keep mechanic notes", "Owner"]
    report = json.loads((output / "story_conversion.json").read_text())
    quest_report = next(t for t in report["tables"] if t["source"] == "quests.txt")
    assert quest_report["ignored_localization_ids"] == [11] and quest_report["untranslated_rows"] == 1
    assert report["runtime_ready"] is False and report["table_loaders_validated"] is True
    assert all(stat.S_IMODE(p.stat().st_mode) == 0o644 for p in output.rglob("*") if p.is_file())
    assert {p.name: p.read_bytes() for p in data.iterdir()} == original


def test_english_without_optional_translation(tmp_path):
    root = tmp_path / "source"
    (root / "Data").mkdir(parents=True)
    write_archive(root / "Data/mmmerge.T.lod", fixtures())
    output = tmp_path / "story"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    assert read_rows(output / "engine/data_tables/npc_greet.txt")["1"][1] == "Welcome\ntraveller"


def test_legacy_unquoted_continuations_weekday_ids_and_empty_topic_slot(tmp_path):
    root = tmp_path / "source"
    data = root / "Data"
    data.mkdir(parents=True)
    original = fixtures()
    original["npctopic.txt"].append(["1795"])
    raw = {
        "npcgreet.txt": "1\tFirst\nSecond\tRepeat\nAgain\t\tResident\n",
        "npctext.txt": "200\tFirst\n\nSecond\tNotes\tOwner\n201\tParty is full\t\tOwner\n",
        "npcnews.txt": "News # \tText\tNotes / Topic\n381 M\tQuestion:\nAnswer\tMonday\n",
    }
    write_archive(data / "mmmerge.T.lod", original, raw_texts=raw)
    output = tmp_path / "story"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    tables = output / "engine/data_tables"
    assert read_rows(tables / "npc_greet.txt")["1"][1:3] == ["First\nSecond", "Repeat\nAgain"]
    assert read_rows(tables / "npc_topic_text.txt")["200"][1:] == ["First\n\nSecond", "Notes", "Owner"]
    assert read_rows(tables / "npc_news.txt")["381"][1:] == ["Question:\nAnswer", "Monday"]
    assert "1795" not in read_rows(tables / "npc_topic.txt")
    report = json.loads((output / "story_conversion.json").read_text())
    assert next(t for t in report["tables"] if t["source"] == "npctopic.txt")["empty_base_topic_ids"] == [1795]
    assert next(t for t in report["tables"] if t["source"] == "npcnews.txt")["normalized_base_weekday_news_ids"] == [381]


@pytest.mark.parametrize("fault", ["duplicate", "encoding", "quotes", "glyphs", "wrapper", "missing"])
def test_invalid_translation_never_publishes_output(tmp_path, fault):
    root = tmp_path / "source"
    data = root / "Data"
    data.mkdir(parents=True)
    write_archive(data / "mmmerge.T.lod", fixtures())
    localized = fixtures()
    if fault == "duplicate":
        localized["quests.txt"].append(localized["quests.txt"][0])
    elif fault == "missing":
        del localized["quests.txt"]
    write_archive(data / "10 LocZHCN.EnglishT.lod", localized, True, fault=fault)
    output = tmp_path / "story"
    assert run(root, output).returncode != 0
    assert not output.exists()


def test_existing_and_overlapping_output_preserved(tmp_path):
    root = tmp_path / "source"
    (root / "Data").mkdir(parents=True)
    write_archive(root / "Data/mmmerge.T.lod", fixtures())
    output = tmp_path / "existing"
    output.mkdir()
    (output / "keep").write_text("untouched")
    assert run(root, output).returncode != 0
    assert (output / "keep").read_text() == "untouched"
    assert run(root, root / "output").returncode != 0
    alias = tmp_path / "alias"
    alias.symlink_to(root, target_is_directory=True)
    assert run(root, alias / "output").returncode != 0
