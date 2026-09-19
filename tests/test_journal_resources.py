"""Native journal conversion and production award visibility with synthetic resources."""
import json
import os
from pathlib import Path
import stat
import subprocess

import pytest

from test_story_resources import read_rows, text, write_archive

TOOL = Path(os.environ.get("JOURNAL_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/journal_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build journal_import first")


def source(root, chinese=True, fault=None, unquoted=False):
    data = root / "Data"
    data.mkdir(parents=True)
    base = {
        "awards.txt": [["A Bit", "Awards", "Sort", "Notes"], ["1", "Won a quest", "2", "Source note"],
                       ["36", "Won %lu games", "4", "Counter"]],
        "history.txt": [["#", "Text", "Time", "Page Title"],
                        ["1", "Opening %31\n\nFinal paragraph", "Forward", "Jadame"], ["2", "", "", ""]],
        "mm7history.txt": [["#", "Text", "Time", "Page Title"],
                           ["1", "Harmondale %30\n\nFinal paragraph", "Act 1", "Harmondale"]],
        "autonote.txt": [["Note bit", "Autonote Text", "Category", "", ""],
                         ["1", "Potion note\nSecond line", "potion", "", ""], ["110", "", "Misc", "", ""],
                         ["394", "Legacy award note", "Award", "", ""], ["696", "0", "Misc", "", ""]],
        "quests.txt": [["Q Bit", "Quest Note Text", "Notes", "Owner"]],
    }
    for id_ in range(492, 523):
        base["autonote.txt"].append([str(id_), f"Source achievement {id_}", "Misc", "", ""])
    for id_ in range(1537, 1555):
        base["quests.txt"].append([str(id_), "", f"Source achievement {id_ - 1032}", ""])
    base["quests.txt"].append(["1712", "New quest", "Cross Continent Q"])  # Optional Owner omitted.
    if fault == "alias_join":
        base["quests.txt"][1][2] = "Different achievement"
    if fault == "missing_alias":
        base["autonote.txt"] = [r for r in base["autonote.txt"] if r[0] != "492"]
    if fault == "duplicate_id":
        base["awards.txt"].append(base["awards.txt"][1][:])
    if fault == "overflow_id":
        base["autonote.txt"][1][0] = "65536"
    if fault == "unknown_category":
        base["autonote.txt"][1][2] = "Future category"
    if fault == "invalid_sort":
        base["awards.txt"][1][2] = "2garbage"
    write_archive(data / "mmmerge.T.lod", base)
    if chinese:
        locale = {k: [r[:] for r in v] for k, v in base.items() if k != "quests.txt"}
        for row in locale["autonote.txt"][1:]:
            if row[1] not in ("", "0"):
                row[1] = f"记录{row[0]}\n第二段"
        locale["autonote.txt"][1][2] = "Misc"  # Preserve the actual Potion category.
        locale["awards.txt"][1][1:3] = ['完成"任务"', "5"]  # Preserve original sort order.
        locale["awards.txt"][2][1] = "获胜%lu次"
        locale["history.txt"][1][1:4] = ["开篇%31\n\n最后一段", "Old trigger", "贾丹姆"]
        locale["mm7history.txt"][1][1:4] = ["历史%30\n\n最后一段", "Wrong act", "哈蒙代尔"]
        if fault == "blank_translation":
            locale["history.txt"][1][1] = ""
        if fault == "locale_coverage":
            locale["awards.txt"].pop(1)
            locale["awards.txt"].append(["99", "旧奖励", "1", ""])
            locale["history.txt"][2][1] = "不可激活的旧章节"
        raw = {}
        if unquoted:
            raw["history.txt"] = (text(locale["history.txt"][:1])
                + "1\t开篇%31\n\n最后一段\tOld trigger\t贾丹姆\n2\t\t\t\n")
            raw["mm7history.txt"] = (text(locale["mm7history.txt"][:1])
                + "1\t历史%30\n\n最后一段\tWrong act\t哈蒙代尔\n")
            raw["autonote.txt"] = (text(locale["autonote.txt"][:1])
                + "1\t记录1\n第二段\tMisc\t\t\n" + text(locale["autonote.txt"][2:]))
        if fault == "interrupted_text":
            raw["history.txt"] = "#\tText\tTime\tPage Title\n1\t正文\n2\t别的章节\tForward\t标题\n"
        if fault == "stray_continuation":
            raw["history.txt"] = text(locale["history.txt"]) + "多余段落\n"
        if fault == "unfinished_text":
            raw["history.txt"] = "#\tText\tTime\tPage Title\n1\t未完成的正文\n"
        write_archive(data / "10 LocZHCN.EnglishT.lod", locale, True, raw_texts=raw)
    return root


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True, umask=0o077)


@pytest.mark.parametrize("chinese", [True, False])
def test_journal_mechanics_award_states_and_source_preserved(tmp_path, chinese):
    root = source(tmp_path / "source", chinese=chinese)
    before = {p: p.read_bytes() for p in root.rglob("*") if p.is_file()}
    output = tmp_path / "journal"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    table = output / "engine/data_tables/english"
    awards = read_rows(table / "awards.txt")
    assert awards["1"][2:] == ["2", "Source note"] and awards["36"][2] == "4"
    assert awards["36"][1] == ("获胜%lu次" if chinese else "Won %lu games")
    history = read_rows(table / "history.txt")
    assert history["1"][2] == "Forward" and history["2"][1] == ""
    assert read_rows(table / "mm7_history.txt")["1"][2] == "Act 1"
    notes = read_rows(table / "autonote.txt")
    assert notes["1"][2] == "potion" and notes["110"][1] == "" and notes["696"][1] == "0"
    import csv
    aliases = {row[0]: row for row in csv.reader((table / "award_display_aliases.txt").open(), delimiter="\t")}
    assert len(aliases) == 31
    assert aliases["mm8.award.autonote.492"][1:4] == ["party_autonote", "492", notes["492"][1]]
    assert aliases["mm8.award.qbit.1537"][1:4] == ["party_qbit", "1537", notes["505"][1]]
    report = json.loads((output / "journal_conversion.json").read_text())
    assert report["typed_validation"] and report["award_state_visibility_validated"]
    assert report["party_award_aliases"] == 31 and report["mm8_history_entries"] == report["mm7_history_entries"] == 1
    assert report["award_category_using_existing_misc_ids"] == [394]
    assert not report["runtime_ready"] and not report["full_gameplay_loop"]
    assert all(p.read_bytes() == b for p, b in before.items())
    assert all(stat.S_IMODE(p.stat().st_mode) == 0o644 for p in output.rglob("*") if p.is_file())


def test_unquoted_chinese_paragraphs_match_quoted_tables(tmp_path):
    outputs = []
    for unquoted in (False, True):
        root = source(tmp_path / str(unquoted) / "source", unquoted=unquoted)
        output = tmp_path / f"journal-{unquoted}"
        result = run(root, output)
        assert result.returncode == 0, result.stderr
        outputs.append(output)
    for file in (outputs[0] / "engine/data_tables/english").iterdir():
        assert file.read_bytes() == (outputs[1] / "engine/data_tables/english" / file.name).read_bytes()
    report = json.loads((outputs[1] / "journal_conversion.json").read_text())
    assert report["unquoted_continuation_lines"] == 5


def test_old_translation_cannot_create_new_awards_or_activate_blank_history(tmp_path):
    root = source(tmp_path / "source", fault="locale_coverage")
    output = tmp_path / "journal"
    assert run(root, output).returncode == 0
    report = json.loads((output / "journal_conversion.json").read_text())
    awards = next(t for t in report["tables"] if t["source"] == "awards.txt")
    assert awards["untranslated_ids"] == [1] and awards["ignored_localization_ids"] == [99]
    table = output / "engine/data_tables/english"
    assert read_rows(table / "awards.txt")["1"][1] == "Won a quest"
    assert read_rows(table / "history.txt")["2"][1] == ""


@pytest.mark.parametrize("fault", ["alias_join", "missing_alias", "duplicate_id", "overflow_id", "unknown_category",
                                   "invalid_sort", "blank_translation", "interrupted_text", "stray_continuation",
                                   "unfinished_text"])
def test_invalid_journals_fail_without_output(tmp_path, fault):
    root = source(tmp_path / "source", fault=fault)
    output = tmp_path / "journal"
    result = run(root, output)
    assert result.returncode != 0 and "Journal conversion failed" in result.stderr
    assert not output.exists()


def test_journal_output_and_source_boundaries(tmp_path):
    root = source(tmp_path / "source")
    existing = tmp_path / "existing"
    existing.mkdir()
    (existing / "keep").write_text("preserve")
    assert run(root, existing).returncode != 0 and (existing / "keep").read_text() == "preserve"
    assert run(root, root / "output").returncode != 0 and not (root / "output").exists()
    archive = root / "Data/mmmerge.T.lod"
    outside = tmp_path / "outside.lod"
    archive.rename(outside)
    archive.symlink_to(outside)
    output = tmp_path / "journal"
    assert run(root, output).returncode != 0 and not output.exists()
