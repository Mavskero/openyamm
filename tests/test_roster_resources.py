"""Roster IDs, patched class aliases and Chinese presentation using synthetic data."""
import copy
import csv
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_story_resources import text, write_archive

TOOL = Path(os.environ.get("ROSTER_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/roster_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build roster_import first")
SKILLS = ("Staff,Sword,Dagger,Axe,Spear,Bow,Mace,Blaster,Shield,Leather,Chain,Plate,Fire,Air,Water,Earth,"
          "Spirit,Mind,Body,Light,Dark,DarkElf,Vampire,Dragon,ID Item,Merchant,Repair,B Building,Meditate,"
          "Perception,Regeneration,Disarm Trap,Dodging,Unarmed,Ident Mon,Armsmaster,Stealing,Alchemy,Learning").split(",")
HEADER = ("#,Name,Class,Birthdate,Picture,Voice,Experience,Level,Might,Intelligence,Personality,Endurance,"
          "Speed,Accuracy,Luck,Fire resist,Air Resist,Water Resist,Earth resist,Mind Resist,Body resist,Skillpoints").split(",")
HEADER += [cell for skill in SKILLS for cell in (skill, "")]
HEADER += SKILLS[12:24] + ["QuestBit", "Equipment - 0 is not a valid item!!"] + [""] * 9 + ["Blurb", "End"]


def write(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text(rows))


def records():
    rows = [[""] * 125, HEADER.copy()]
    for id_, name, class_ in [(0, "Placeholder", "Placeholder"), (1, "First", "cleric2"),
                              (2, "Second", "5"), (37, "Template", "cleric")]:
        row = [""] * 125
        row[:8] = [str(id_), name, class_, "1140", "0", "2", "300", "3"]
        row[8:22] = [str(i + 1) for i in range(14)]
        row[46:48] = ["B ", "2"]
        row[100] = "1"
        row[112] = str(400 + id_)
        row[113:116] = ["2078", "78", "132"]
        row[123] = 'Original "biography"\nSecond line.'
        row[124] = "x"
        if id_ in (0, 37):
            row[113:123] = [""] * 10
        rows.append(row)
    return rows


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    (root / "Data/Tables").mkdir(parents=True)
    (root / "Scripts/General").mkdir(parents=True)
    (root / "Scripts/General/MiscTweaks.lua").write_text(
        'local ClassConns = {\n cleric = 4,\n cleric2 = 6 -- merged promotion\n}\n')
    write(root / "Data/Tables/Character portraits.txt", [["#"] + [""] * 26, ["0"] + [""] * 26])
    tables = assets / "engine/data_tables"
    names = ["Cleric", "Priest", "PriestLight"]
    write(tables / "class_skills.txt", [[""] + names, ["Fire", "B", "E", "G"]])
    write(tables / "class_extra.txt", [["#", "Kind", "Step", "Note"]]
          + [[str(4 + i), "2", str(i), name] for i, name in enumerate(names)])
    write(tables / "class_multipliers.txt", [["Class", "HP", "HPFactor", "SP", "SPFactor", "Mode"]]
          + [[name, "20", "3", "10", "4", "Personality"] for name in names])
    write(tables / "items.txt", [[str(i), "item", f"Item {i}", "1", "0", "Misc", "0", "0", "0", "0",
                                   "Unknown", "0", "0", "0", "0", "0", "Description"] for i in (78, 132)])
    write(tables / "npc_topic.txt", [["#", "Topic", "", "", "", "", "", "Base Topic"],
                                      ["601", "加入", "0", "", "", "First", "", "Roster Join Event"],
                                      ["602", "Join", "0", "", "", "Second", ""]])
    write(tables / "npc_topic_text.txt", [[str(i), f"对话{i}"] for i in range(200, 204)])
    write_archive(root / "Data/mmmerge.T.lod", {"roster.txt": records()})
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def read(output):
    with (output / "engine/data_tables/roster.txt").open() as stream:
        return {int(row[0]): row for row in list(csv.reader(stream, delimiter="\t"))[2:]}


def test_chinese_keeps_merged_class_equipment_and_creation_slots(prepared):
    root, assets, output = prepared
    localized = copy.deepcopy(records())
    localized[3][1] = "\x0e \x0e光\x07 \x0e明\x07\x0f牧师"
    localized[3][123] = '他说："你好"\n第二段。'
    localized[3][2] = "5"  # Old localization cannot downgrade the class.
    localized[3][8] = "999"
    localized[3][113] = "6999"
    extra = localized[3].copy()
    extra[0] = "60000"
    localized.append(extra)
    write_archive(root / "Data/10 LocZHCN.EnglishT.lod", {"roster.txt": localized}, True)
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    rows = read(output)
    assert set(rows) == {1, 2, 37}
    assert rows[1][1:3] == ["光明牧师", "6"] and rows[2][2] == "5" and rows[37][2] == "4"
    assert rows[1][123] == '他说："你好"\n第二段。'
    assert rows[1][8:123] == records()[3][8:123]
    report = json.loads((output / "roster_conversion.json").read_text())
    assert report["active_roster"] == report["recruitment_checks"] == 3
    assert report["starting_items"] == 6 and report["resolved_join_offers"] == 2
    assert report["ignored_translation_mechanics"] == 3 and report["ignored_extra_translations"] == 1
    assert report["restored_chinese_glyphs"] == 4 and report["inactive_ids"] == [0]
    assert not report["runtime_ready"] and not report["original_enchantment_rolls_validated"]
    assert all(p.read_bytes() == content for p, content in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


def test_english_numeric_classes_and_missing_translation_cells(prepared):
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    assert read(prepared[2])[1][1] == "First"


@pytest.mark.parametrize("mastery", ["N", "", "  "])
def test_positive_level_without_higher_mastery_is_basic(prepared, mastery):
    rows = records()
    rows[3][46] = mastery
    write_archive(prepared[0] / "Data/mmmerge.T.lod", {"roster.txt": rows})
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    assert read(prepared[2])[1][46:48] == [mastery, "2"]


@pytest.mark.parametrize("fault", ["header", "width", "duplicate", "zero", "class", "item", "item_multiple",
                                    "negative", "mastery", "no_skill_level", "spells", "portrait", "level"])
def test_bad_source_has_no_partial_output(prepared, fault):
    root, assets, output = prepared
    rows = records()
    if fault == "header":
        rows[1][46] = "Water"
    elif fault == "width":
        rows[3].pop()
    elif fault == "duplicate":
        rows.append(rows[3].copy())
    elif fault == "zero":
        rows[2][1] = "Real character lost by zero ID"
    else:
        column, value = {"class": (2, "Imaginary"), "item": (113, "2999"), "item_multiple": (113, "2000"),
                         "negative": (8, "-1"), "mastery": (46, "Z"), "no_skill_level": (47, "0"),
                         "spells": (100, "12"), "portrait": (4, "999"), "level": (7, "0")}[fault]
        rows[3][column] = value
    write_archive(root / "Data/mmmerge.T.lod", {"roster.txt": rows})
    assert run(prepared).returncode != 0
    assert not output.exists()


@pytest.mark.parametrize("fault", ["expression", "duplicate", "missing", "unavailable_class", "extra_block"])
def test_declarative_class_mapping_is_never_executed(prepared, fault):
    root, _, output = prepared
    script = root / "Scripts/General/MiscTweaks.lua"
    content = script.read_text()
    if fault == "expression":
        content = content.replace("cleric2 = 6", "cleric2 = execute()")
    elif fault == "duplicate":
        content = content.replace("cleric2 = 6", "cleric = 4,\n cleric2 = 6")
    elif fault == "missing":
        content = "-- absent"
    elif fault == "unavailable_class":
        content = content.replace("cleric2 = 6", "cleric2 = 255")
    else:
        content += "\n" + content
    script.write_text(content)
    assert run(prepared).returncode != 0
    assert not output.exists()


def test_output_and_input_symlink_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    target = tmp_path / "outside.lua"
    script = root / "Scripts/General/MiscTweaks.lua"
    script.rename(target)
    script.symlink_to(target)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()
