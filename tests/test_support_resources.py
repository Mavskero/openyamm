"""Portrait choices, original reagent recipes and additional interface metadata."""
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_doll_resources import read
from test_roster_resources import write

TOOL = Path(os.environ.get("SUPPORT_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/support_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build support_import first")
UI_HEADER = ["#", "Lod name", "D Lod name", "Show blank hostile indicator", "Hostile indicator Y",
             "Hostile indicator X offset", "Selring on top", "Selring Y", "Selring X offset", "Notes"]
TABLE_NAMES = {"MonPortraits.txt": "monster_portraits.txt", "Reagent settings.txt": "reagent_settings.txt",
               "Additional UI.txt": "additional_ui.txt"}


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    source = root / "Data/Tables"
    write(source / "MonPortraits.txt", [["#", "Pic index", "Note"],
          ["1", "100, 200, 100", "Peasant"], ["2", "0", "No portrait"]])
    write(source / "Reagent settings.txt", [["Reagent Id", "Item Id", "Result item id", "Notes"],
          ["1", "200", "222", "Reagent"], ["2", "201", "223", "Imported herb"]])
    write(source / "Additional UI.txt", [UI_HEADER,
          ["1", "default", "", "-", "458", "0", "-", "389", "0", "Original"],
          ["2", "7UI.icons.lod", "", "X", "378", "-3", "x", "384", "-4", "Variant"]])
    skin = root / "Data/Additional UI/7UI.icons.lod"
    skin.parent.mkdir()
    skin.write_bytes(b"Opaque archive fixture: metadata check does not decode art")
    base = assets / "engine/data_tables"
    items = []
    for id_, kind, power in [(200, "Reagent", 1), (201, "Herb", 13), (220, "Bottle", 0),
                             (221, "Bottle", 0), (222, "Bottle", 0), (223, "Bottle", 0)]:
        items.append([str(id_), "image", f"物品{id_}", "1", kind, "Misc", str(power), "0", "0", "0",
                      "未鉴定", "0", "0", "0", "0", "0", "中文物品说明"])
    write(base / "items.txt", items)
    write(base / "potion_settings.txt", [["Potion Id", "Item Id", "Req. mastery", "Drinkable", "Usable", "Notes"]]
          + [[str(i), str(220 + i), "0", "x", "", "Potion"] for i in range(4)])
    monsters = [[str(i), f"怪物{i}", "Peasant", "1", "10", "0", "0"] + ["0"] * 32 for i in range(1, 7)]
    write(base / "monster_data.txt", monsters)
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def test_choices_zero_groups_both_mixing_directions_and_signed_ui_offsets(prepared):
    root, assets, output = prepared
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    for source, target in TABLE_NAMES.items():
        assert read(root / "Data/Tables" / source) == read(output / "engine/data_tables" / target)
    report = json.loads((output / "support_conversion.json").read_text())
    assert report["portrait_groups"] == report["distinct_portrait_ids"] == 2
    assert report["portrait_choices"] == 3 and report["zero_portrait_groups"] == 1
    assert report["portrait_selection_checks"] == 12 and report["reagent_mix_checks"] == 4
    assert report["present_interface_archive_files"] == report["declared_interface_archives"] == 1
    assert report["portrait_images_exported"] == report["ui_images_exported"] == 0
    assert not report["runtime_ready"] and not report["ui_art_validated"] and not report["alchemy_ui_validated"]
    assert all(p.read_bytes() == data for p, data in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["portrait_order", "portrait_empty", "portrait_zero_mix", "portrait_garbage",
                                    "reagent_order", "reagent_duplicate", "reagent_result", "ui_order", "ui_marker",
                                    "ui_coordinate", "ui_path"])
def test_invalid_source_is_not_published(prepared, fault):
    root, _, output = prepared
    filename, row, column, value = {
        "portrait_order": ("MonPortraits.txt", 1, 0, "2"),
        "portrait_empty": ("MonPortraits.txt", 1, 1, "100,"),
        "portrait_zero_mix": ("MonPortraits.txt", 1, 1, "0,100"),
        "portrait_garbage": ("MonPortraits.txt", 1, 1, "100,skip"),
        "reagent_order": ("Reagent settings.txt", 1, 0, "3"),
        "reagent_duplicate": ("Reagent settings.txt", 2, 1, "200"),
        "reagent_result": ("Reagent settings.txt", 1, 2, "999"),
        "ui_order": ("Additional UI.txt", 1, 0, "2"),
        "ui_marker": ("Additional UI.txt", 1, 3, "yes"),
        "ui_coordinate": ("Additional UI.txt", 1, 4, "99999999"),
        "ui_path": ("Additional UI.txt", 2, 1, "../outside.lod"),
    }[fault]
    path = root / "Data/Tables" / filename
    rows = read(path)
    rows[row][column] = value
    write(path, rows)
    assert run(prepared).returncode != 0 and not output.exists()


def test_missing_ui_archive_is_reported_as_unvalidated_metadata(prepared):
    (prepared[0] / "Data/Additional UI/7UI.icons.lod").unlink()
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    report = json.loads((prepared[2] / "support_conversion.json").read_text())
    assert report["present_interface_archive_files"] == 0 and not report["ui_art_validated"]


def test_missing_monster_tier_is_rejected(prepared):
    path = prepared[1] / "engine/data_tables/monster_data.txt"
    write(path, read(path)[:-1])
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_output_and_symlink_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    path = root / "Data/Tables/MonPortraits.txt"
    outside = tmp_path / "outside.txt"
    path.rename(outside)
    path.symlink_to(outside)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()
