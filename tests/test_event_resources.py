"""Import original binary EVT/STR from synthetic LODs; never ship game data in tests."""
import csv
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import subprocess

import pytest

from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("EVENT_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/event_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build event_import first")


def record(event_id, step, opcode, payload=b"\0"):
    return bytes([4 + len(payload)]) + struct.pack("<HBB", event_id, step, opcode) + payload


def archive(path, entries):
    lod(path, {name: wrapped(name, data) for name, data in entries.items()})


def fixture(tmp_path):
    root, base = tmp_path / "source", tmp_path / "base"
    (root / "Data").mkdir(parents=True)
    tables = base / "engine/data_tables"
    (tables / "english").mkdir(parents=True)
    map_row = ["0"] * 35
    map_row[0:3], map_row[34] = ["1", "新索匹格镇", "oute3.odm"], "mm6"
    rows = {
        "house_data.txt": [["325", "0", "0", "0", "0", "神殿"]],
        "npc_topic_text.txt": [["1", "对话"]], "npc.txt": [["1", "居民"]],
        "npc_topic.txt": [["1", "任务"]], "english/quests.txt": [["1105", "送信"]],
        "map_stats.txt": [map_row],
    }
    for name, table in rows.items():
        with (tables / name).open("w", newline="", encoding="utf-8") as out:
            csv.writer(out, delimiter="\t").writerows(table)
    events = b"".join([
        record(1, 0, 4, b"\1"), record(1, 1, 1),
        record(2, 0, 29, struct.pack("<I", 2)), record(2, 1, 1),
        record(3, 0, 2, struct.pack("<I", 325)), record(3, 1, 1),
        record(4, 0, 6, struct.pack("<iiiiiiHB", 123, 456, 10, 512, 0, 0, 417, 5) + b"next.blv\0"),
        record(4, 1, 1),
        record(5, 0, 16, struct.pack("<HI", 16, 1105)), record(5, 1, 1),
    ])
    entries = {"OUTE3.EVT": events, "OUTE3.STR": b"\0Temple\0Line one\nLine two\0"}
    archive(root / "Data/mm6.T.lod", entries)
    archive(root / "Data/mmmerge.T.lod", {"Global.EVT": record(500, 0, 1)})
    localized = {"oute3.str": "\0神殿\0第一行\n第二行\0".encode("gb2312")}
    archive(root / "Data/10 LocZHCN.EnglishT.lod", localized)
    return root, base, entries, localized


def run(root, base, output, map_stem="oute3"):
    return subprocess.run([str(TOOL), str(root), str(base), str(output), map_stem],
                          capture_output=True, text=True, umask=0o077)


def digest_tree(root):
    return {p.relative_to(root): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob("*") if p.is_file()}


def test_merged_ids_chinese_nul_slots_and_input_preservation(tmp_path):
    root, base, _, _ = fixture(tmp_path)
    before = digest_tree(root), digest_tree(base)
    output = tmp_path / "converted"
    result = run(root, base, output, "OUTE3")
    assert result.returncode == 0, result.stderr
    manifest = json.loads((output / "event_conversion.json").read_text())
    assert manifest["legacy_event_version"] == 8
    assert manifest["runtime_ready"] is False and manifest["mmextension_lua_overlays_applied"] is False
    global_program, local = manifest["programs"]
    assert global_program["path"] == "engine/events/Global.lua"
    assert local["path"] == "worlds/mm6/events/maps/oute3.lua"
    assert local["source_events"] == 5 and local["source_instructions"] == 10
    assert local["string_slots"] == 3 and local["lua_load_passed"] is True
    assert local["str_archive"] == "10 LocZHCN.EnglishT.lod"
    lua = (output / local["path"]).read_text()
    assert '"神殿"' in lua and '第一行\\n第二行' in lua
    assert 'QBit(1105)' in lua  # Already merged: do not remap the MM6 QBit a second time.
    assert '417' in lua and 'next.blv' in lua and '325' in lua
    assert (output / "engine/scripts/common/event_support.lua").is_file()
    assert (digest_tree(root), digest_tree(base)) == before
    assert all(stat.S_IMODE(p.stat().st_mode) == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault,message", [
    ("unterminated_str", "Unterminated source STR"),
    ("wrong_slots", "slot count differs"),
    ("truncated_evt", "Malformed EVT"),
    ("short_add", "payload"),
    ("unterminated_map", "Unterminated opcode string payload"),
    ("short_20", "payload"),
    ("short_27", "payload"),
    ("short_28", "payload"),
    ("unknown_0", "Unimplemented opcode 0"),
    ("unknown_69", "Unimplemented opcode 69"),
    ("localized_evt", "Language archive must not replace"),
])
def test_rejects_damaged_or_unimplemented_events_without_partial_output(tmp_path, fault, message):
    root, base, entries, localized = fixture(tmp_path)
    if fault == "unterminated_str":
        localized["oute3.str"] = localized["oute3.str"][:-1]
    elif fault == "wrong_slots":
        localized["oute3.str"] += b"\0"
    elif fault == "localized_evt":
        localized["oute3.evt"] = record(1, 0, 1)
    elif fault == "truncated_evt":
        entries["OUTE3.EVT"] = entries["OUTE3.EVT"][:-1]
    elif fault == "short_add":
        entries["OUTE3.EVT"] = record(1, 0, 16, b"\1")
    elif fault == "unterminated_map":
        entries["OUTE3.EVT"] = record(1, 0, 6, bytes(27) + b"next.blv")
    elif fault.startswith("short_"):
        opcode = int(fault.split("_")[1])
        minimum_payload = {20: 5, 27: 2, 28: 1}[opcode]
        entries["OUTE3.EVT"] = record(1, 0, opcode, bytes(minimum_payload - 1))
    else:
        entries["OUTE3.EVT"] = record(1, 0, int(fault.split("_")[1]), b"\1\1")
    archive(root / "Data/mm6.T.lod", entries)
    archive(root / "Data/10 LocZHCN.EnglishT.lod", localized)
    before = digest_tree(root), digest_tree(base)
    output = tmp_path / "converted"
    result = run(root, base, output)
    assert result.returncode != 0
    assert message in result.stderr
    assert not output.exists()
    assert (digest_tree(root), digest_tree(base)) == before


@pytest.mark.parametrize("opcode,payload", [(20, b"\x47\0\0\0\1"), (27, b"\1\1"), (28, b"\1")])
def test_original_mm8_ignored_commands_preserve_the_following_instruction(tmp_path, opcode, payload):
    root, base, entries, _ = fixture(tmp_path)
    entries["OUTE3.EVT"] = (record(21, 0, opcode, payload)
                           + record(21, 1, 15, b"\x15\1") + record(21, 2, 1))
    archive(root / "Data/mm6.T.lod", entries)
    output = tmp_path / "converted"
    result = run(root, base, output)
    assert result.returncode == 0, result.stderr
    lua = (output / "worlds/mm6/events/maps/oute3.lua").read_text()
    assert f"do end -- Original MM8 ignores command {opcode}" in lua
    assert "evt.SetDoorState(21, DoorAction.Close)" in lua
    manifest = json.loads((output / "event_conversion.json").read_text())
    assert manifest["programs"][1]["source_instructions"] == 3
    assert manifest["programs"][1]["lua_load_passed"] is True


def test_protects_existing_output_and_both_input_roots(tmp_path):
    root, base, _, _ = fixture(tmp_path)
    existing = tmp_path / "existing"
    existing.mkdir()
    marker = existing / "keep"
    marker.write_text("untouched")
    for output in (existing, root / "output", base / "output"):
        assert run(root, base, output).returncode != 0
    assert marker.read_text() == "untouched"
    assert not (root / "output").exists() and not (base / "output").exists()


@pytest.mark.parametrize("map_stem", ["absent", "../oute3"])
def test_requires_a_safe_registered_map(tmp_path, map_stem):
    root, base, _, _ = fixture(tmp_path)
    output = tmp_path / "converted"
    assert run(root, base, output, map_stem).returncode != 0
    assert not output.exists()


@pytest.mark.parametrize("stems", [("oute3", "absent"), ("oute3", "OUTE3")])
def test_multi_map_request_rejects_missing_or_duplicate_member(tmp_path, stems):
    root, base, _, _ = fixture(tmp_path)
    output = tmp_path / "converted"
    result = subprocess.run([str(TOOL), str(root), str(base), str(output), *stems], capture_output=True)
    assert result.returncode != 0
    assert not output.exists()


def test_multi_map_selection_keeps_one_global_and_requested_local_programs(tmp_path):
    root, base, entries, _ = fixture(tmp_path)
    table = base / "engine/data_tables/map_stats.txt"
    with table.open(newline="", encoding="utf-8") as stream:
        first = next(csv.reader(stream, delimiter="\t"))
    second = list(first)
    second[0:3] = ["2", "地牢", "6d01.blv"]
    with table.open("a", newline="", encoding="utf-8") as stream:
        csv.writer(stream, delimiter="\t").writerow(second)
    entries["6d01.evt"] = record(1, 0, 1)
    archive(root / "Data/mm6.T.lod", entries)
    output = tmp_path / "converted"
    result = subprocess.run([str(TOOL), str(root), str(base), str(output), "oute3", "6d01"],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / "event_conversion.json").read_text())
    assert report["map_filters"] == ["6d01", "oute3"]
    assert [p["path"] for p in report["programs"]] == [
        "engine/events/Global.lua", "worlds/mm6/events/maps/oute3.lua", "worlds/mm6/events/maps/6d01.lua"]


def test_hint_only_event_with_absent_text_retains_inert_registration(tmp_path):
    root, base, entries, _ = fixture(tmp_path)
    entries["OUTE3.EVT"] += record(28, 0, 4, b"\x8c") + record(28, 0, 1)
    archive(root / "Data/mm6.T.lod", entries)
    output = tmp_path / "converted"
    result = run(root, base, output)
    assert result.returncode == 0, result.stderr
    lua = (output / "worlds/mm6/events/maps/oute3.lua").read_text()
    assert "RegisterNoOpEvent(28," in lua
    manifest = json.loads((output / "event_conversion.json").read_text())
    assert manifest["programs"][1]["source_events"] == 6
