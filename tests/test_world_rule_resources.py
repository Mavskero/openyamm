"""World rule registry joins, source replacement semantics and travel boundaries."""
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_bolster_resources import source_rows, write_source
from test_doll_resources import read
from test_roster_resources import write

TOOL = Path(os.environ.get("WORLD_RULE_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/world_rule_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build world_rule_import first")
NAMES = {"Continent settings.txt": "continent_settings.txt", "HW water textures.txt": "hw_water_textures.txt",
         "House exits.txt": "house_exits.txt", "House rules.txt": "house_rules.txt",
         "Outdoor travels.txt": "outdoor_travels.txt", "Overlay.txt": "overlay.txt",
         "TownPortalSwitch.txt": "town_portal_switch.txt", "Transport index.txt": "transport_index.txt",
         "Transport locations.txt": "transport_locations.txt"}
CONTINENT_HEADER = ["#", "Note", "Rep affects guards", "Rep affects shops", "Rep affects NPC", "Tell prof news",
                    "NPC Followers", "Saturation", "Softness", "Death movie", "Specific water", "Death map 1",
                    "X", "Y", "Z", "Dir", "Death map 2", "X", "Y", "Z", "Dir",
                    'Skies (from clear to storm, divided by ",")', "Loading pics"]
HOUSE_SECTIONS = ["Weapon shops Standart", "Weapon shops Special", "Armor shops Standart", "Armor shops Special",
                  "Magic shops Standart", "Magic shops Special", "Alchem shops Standart", "Alchem shops Special",
                  "Spellbook shops", "Stables", "Boats", "Training halls", "Arcomage in taverns"]


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    source = root / "Data/Tables"
    tables = {
        "Continent settings.txt": [CONTINENT_HEADER,
            ["1", "Continent", "x", "", "x", "x", "x", "0.95", "1.15", "death", "water", "area.odm",
             "-100", "200", "0", "-12", "", "", "", "", "", "sky1, sky2", "load1.pcx,load2.pcx"]],
        "HW water textures.txt": [["SW texture", "HW texture prefix"], ["water", "hdwater"]],
        "House exits.txt": [["NPC pics:", "1561"], ["Free NPC:", "31", ""], ["Free topic:", "750"],
                            ["Map name", "X", "Y", "Z"], ["sewer.blv", "-100", " 200", "30"]],
        "Outdoor travels.txt": [["Key map", "Up", "Side", "Days", "Down", "Side", "Days", "Left", "Side", "Days",
                                  "Right", "Side", "Days", "StraightTravel", "Notes", ""],
                                 ["area.odm", "area.odm", "down", "5"] + [""] * 9 + ["x", "Area", ""]],
        "Overlay.txt": [["Id", "Type", "SFTGroup"], ["1020", "0", "null"]],
        "TownPortalSwitch.txt": [["#", "Map", "X", "Y", "Z", "Direction", "LookAngle", "IconName", "Icon X",
                                   "Icon Y", "IconWidth", "IconHeight", "QBitIndex", "Description", ""],
                                  ["@", "townport", "300"],
                                  ["0", "1", "-100", "200", "30", "512", "0", "portal", "10", "20", "", "",
                                   "301", "English source note", ""],
                                  ["@", "global", "309"],
                                  ["0", "0", "0", "0", "0", "0", "0", "world", "0", "0", "32", "24", "309",
                                   "Jadame"]],
        "Transport index.txt": [["2D Event", "1", "2", "3"], ["1", "0", "-1", "0"]],
        "Transport locations.txt": [["#", "Map", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
                                      "Sunday", "DaysCount", "X", "Y", "Z", "Direction", "QBit"],
                                     ["0", "area.odm", "x", "-", "-", "x", "-", "-", "-", "2", "100", "200", "30",
                                      "-512", "0"]],
    }
    rules = [["Index by type", "Quality", "Items1", "Items2", "Items3", "Items4", "Quality", "Items1", "Items2",
              "Items3", "Items4", " ", " "]]
    for index, name in enumerate(HOUSE_SECTIONS):
        header = [name]
        if index in (9, 10):
            header += ["Loc1", "Loc2", "Loc3", "Loc4"]
        elif index == 11:
            header += ["Max level"]
        elif index == 12:
            header += ["Tower to win", "Res to win", "Tower at start", "Wall at start", "Res1 per turn", "Res2", "Res3",
                       "Res1 at start", "Res2", "Res3", "Ai", "'Rules' text index"]
        width = [6, 6, 11, 11, 2, 2, 2, 2, 2, 5, 5, 2, 13][index]
        row = ["1"] + ["0" if index in (9, 10) else "2"] * (width - 1)
        rules.extend([header, row])
        if index == 7:
            rules[-1] = ["1", "1"]
            rules.append(["1", "2"])  # The original file repeats Alchem Special 19: later quality wins.
    tables["House rules.txt"] = rules
    for name, rows in tables.items():
        write_source(source / name, rows)
    base = assets / "engine/data_tables"
    write(base / "map_stats.txt", [[str(i), name, file] + ["0"] * 31 + ["mm6"]
          for i, name, file in [(1, "中文区域", "area.odm"), (2, "下水道", "sewer.blv")]])
    houses = []
    for id_, kind in [(1, "Stables"), (2, "Alchemist")]:
        row = [str(id_), "1", kind, "1", "1", "中文房屋", "店主", "称号", "1"] + ["0"] * 15
        if id_ == 2:
            row[20:24] = ["1", "2", "-1", "进入下水道"]
        houses.append(row)
    write(base / "house_data.txt", houses)
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def test_original_rows_duplicate_rules_and_zero_route_are_preserved(prepared):
    root, assets, output = prepared
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    for source, target in NAMES.items():
        assert read(output / "engine/data_tables" / target) == source_rows(root / "Data/Tables" / source)
    report = json.loads((output / "world_rule_conversion.json").read_text())
    assert report["continents"] == report["water_bindings"] == 1
    assert report["house_rule_sections"] == 13 and report["replaced_house_rule_rows"] == 1
    assert report["outdoor_origins"] == report["outdoor_directions"] == 1
    assert report["portal_groups"] == 2 and report["portal_map_destinations"] == 1
    assert report["transport_locations"] == report["legacy_transport_indices"] == 1
    assert report["applied_house_routes"] == report["applied_extra_exits"] == report["house_exit_positions"] == 1
    assert report["typed_validation"] and report["images_exported"] == report["movies_exported"] == 0
    assert not report["legacy_transport_indices_applied"] and not report["original_arrival_side_parity"]
    assert not report["rendered_travel_validated"] and not report["runtime_ready"]
    assert all(p.read_bytes() == data for p, data in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["continent_id", "continent_marker", "continent_decimal", "death_map", "water",
                                    "water_duplicate", "house_exit_map", "house_exit_coordinate", "route_index",
                                    "transport_map", "weekday", "outdoor_map", "outdoor_side", "outdoor_extension",
                                    "portal_map", "portal_dimension", "portal_identity", "active_overlay"])
def test_invalid_world_rule_is_not_published(prepared, fault):
    name, row, column, value = {
        "continent_id": ("Continent settings.txt", 1, 0, "2"),
        "continent_marker": ("Continent settings.txt", 1, 2, "X"),
        "continent_decimal": ("Continent settings.txt", 1, 7, "nan"),
        "death_map": ("Continent settings.txt", 1, 11, "missing.odm"),
        "water": ("Continent settings.txt", 1, 10, "unknown"),
        "water_duplicate": ("HW water textures.txt", 1, 0, "water"),
        "house_exit_map": ("House exits.txt", 4, 0, "missing.blv"),
        "house_exit_coordinate": ("House exits.txt", 4, 1, "bad"),
        "route_index": ("Transport index.txt", 1, 1, "1"),
        "transport_map": ("Transport locations.txt", 1, 1, "missing.odm"),
        "weekday": ("Transport locations.txt", 1, 2, "yes"),
        "outdoor_map": ("Outdoor travels.txt", 1, 1, "missing.odm"),
        "outdoor_side": ("Outdoor travels.txt", 1, 2, "northwest"),
        "outdoor_extension": ("Outdoor travels.txt", 1, 15, "unknown"),
        "portal_map": ("TownPortalSwitch.txt", 2, 1, "99"),
        "portal_dimension": ("TownPortalSwitch.txt", 2, 10, "0"),
        "portal_identity": ("TownPortalSwitch.txt", 2, 0, "1"),
        "active_overlay": ("Overlay.txt", 1, 2, "unbound-sprite"),
    }[fault]
    path = prepared[0] / "Data/Tables" / name
    rows = source_rows(path)
    rows[row][column] = value
    if fault == "water_duplicate":
        rows.append(rows[1])
    write_source(path, rows)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


@pytest.mark.parametrize("fault", ["section_order", "numeric_hole", "unknown_house_route"])
def test_invalid_house_rules_are_not_published(prepared, fault):
    path = prepared[0] / "Data/Tables/House rules.txt"
    rows = source_rows(path)
    if fault == "section_order":
        rows[1][0] = "Weapon shops Special"
    elif fault == "numeric_hole":
        rows[2][1] = ""
    else:
        index = next(i for i, row in enumerate(rows) if row[0] == "Stables")
        rows[index + 1][1] = "255"
    write_source(path, rows)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_output_and_symlink_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    path = root / "Data/Tables/Continent settings.txt"
    outside = tmp_path / "outside.txt"
    path.rename(outside)
    path.symlink_to(outside)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()
