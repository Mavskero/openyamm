"""Bolster source identities, literal formula notes and production registry joins."""
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_doll_resources import read
from test_roster_resources import write

TOOL = Path(os.environ.get("BOLSTER_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/bolster_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build bolster_import first")
MAP_HEADER = ["#", "Note", "Continent", "Bolster kind", "Spells", "Summons", "Weather", "Bolster extra",
              "Profs max rarity", "Custom sky"]
MON_HEADER = ["#", "Note", "Type", "ExtraType", "Creed", "Gender", "Style", "Pref magic", "No bounty hunt",
              "New ranged attacks", "New spells", "Size affects HP", "Replicate", "New summons", "Summon Id",
              "Extra points", "Max HP Boost (%)"]
TABLE_NAMES = {"Bolster - formulas.txt": "bolster_formulas.txt", "Bolster - maps.txt": "bolster_maps.txt",
               "Bolster - monsters.txt": "bolster_monsters.txt"}


def write_source(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(("\r\n".join("\t".join(r) for r in rows) + "\r\n").encode("cp1252"))


def source_rows(path):
    return [line.split("\t") for line in path.read_text("cp1252").splitlines()]


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    source = root / "Data/Tables"
    write_source(source / "Bolster - formulas.txt", [["Target", "Stat", "Formula", "Notes"],
                 ["def", "HP", "HP + max(HP, 1)", '"Target" values remain literal'],
                 ["1", "HP", "HP * 2", "Family override"], ["", "", "", "Additional notes"]])
    write_source(source / "Bolster - maps.txt", [MAP_HEADER,
                 ["0", "", "", "0", "-", "-", "", "0", "0", ""],
                 ["1", "Area", "3", "LowerToEqual", "x", "x", "x", "0", "10", "sunset"],
                 ["2", "Dungeon", "1", "NoBolster", "-", "-", "-", "0", "", ""]])
    write_source(source / "Bolster - monsters.txt", [MON_HEADER,
                 ["0", "", "0", "", "0", "", "0", "0"] + ["-"] * 6 + [""] * 3,
                 ["1", "Undead", "Undead", "Elemental", "Dark", "", "Magic", "Dark"]
                 + ["-", "x", "x", "-", "-", "x", "2", "", "300"],
                 ["2", "Peasant", "Human", "Peasant,NoArena", "Peasant", "F", "Wimpy", "0"]
                 + ["x", "-", "-", "-", "-", "-", "", "", "100"],
                 ["3", "Dragon", "Dragon", "", "Neutral", "", "Strength", "Fire"]
                 + ["-", "x", "-", "x", "-", "-", "", "", "500"]])
    write_source(source / "Continent settings.txt", [["#", "Notes"], ["1", "Jadame"], ["3", "Enroth"]])
    base = assets / "engine/data_tables"
    write(base / "map_stats.txt", [[str(i), "测试区域", f"area{i}.odm"] + ["0"] * 31 + ["mm6"] for i in (1, 2)])
    write(base / "monster_data.txt", [[str(i), "怪物", f"Monster{i}", "1", "10", "0", "0"]
                                      + ["0"] * 32 for i in range(1, 5)])
    write(base / "monster_descriptors.txt", [[str(i - 1), "160", "60", "200", "0", "0", "255", "126", "0"]
                                            + ["0"] * 4 + [f"Monster{i}"] + ["Frame"] * 8 for i in range(1, 5)])
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def test_literal_notes_source_flags_continents_and_uncovered_monster(prepared):
    root, assets, output = prepared
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    for source, target in TABLE_NAMES.items():
        assert source_rows(root / "Data/Tables" / source) == read(output / "engine/data_tables" / target)
    report = json.loads((output / "bolster_conversion.json").read_text())
    assert report["formulas"] == 2 and report["formula_comment_rows"] == 1
    assert report["map_settings"] == 3 and report["monster_settings"] == 4
    assert report["maps_with_random_professions"] == report["summon_references"] == 1
    assert report["monster_ids_without_source_settings"] == [4]
    assert report["source_taxonomy_without_runtime_kind_flag"] == ["Human"]
    assert report["map_continents_applied"] and report["existing_monster_kind_flags_applied"]
    assert report["formula_syntax_checked"] and not report["formulas_executed"]
    assert not report["runtime_uses_imported_formulas"] and not report["runtime_ready"]
    assert all(p.read_bytes() == data for p, data in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


def test_formula_is_compiled_without_execution(prepared):
    path = prepared[0] / "Data/Tables/Bolster - formulas.txt"
    rows = source_rows(path)
    rows[1][2] = 'return error("must not execute during import")'
    write_source(path, rows)
    result = run(prepared)
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("fault", ["formula_syntax", "formula_duplicate", "formula_target", "formula_stat",
                                    "map_duplicate", "map_continent", "map_kind", "monster_kind", "extra_kind",
                                    "monster_summon", "monster_negative_shift", "marker"])
def test_invalid_definition_is_not_published(prepared, fault):
    filename, row, column, value = {
        "formula_syntax": ("Bolster - formulas.txt", 1, 2, "HP +"),
        "formula_duplicate": ("Bolster - formulas.txt", 2, 0, "def"),
        "formula_target": ("Bolster - formulas.txt", 2, 0, "99"),
        "formula_stat": ("Bolster - formulas.txt", 1, 1, "UnknownStat"),
        "map_duplicate": ("Bolster - maps.txt", 2, 0, "0"),
        "map_continent": ("Bolster - maps.txt", 2, 2, "99"),
        "map_kind": ("Bolster - maps.txt", 2, 3, "UnknownKind"),
        "monster_kind": ("Bolster - monsters.txt", 2, 2, "UnknownKind"),
        "extra_kind": ("Bolster - monsters.txt", 2, 3, "UnknownKind"),
        "monster_summon": ("Bolster - monsters.txt", 2, 14, "999"),
        "monster_negative_shift": ("Bolster - monsters.txt", 2, 15, "-1"),
        "marker": ("Bolster - maps.txt", 2, 4, "X"),
    }[fault]
    path = prepared[0] / "Data/Tables" / filename
    rows = source_rows(path)
    rows[row][column] = value
    write_source(path, rows)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_output_and_symlink_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    path = root / "Data/Tables/Bolster - formulas.txt"
    outside = tmp_path / "outside.txt"
    path.rename(outside)
    path.symlink_to(outside)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()
