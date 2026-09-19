"""Owned MM8 binary profile, synthetic art/house rules, and shared card mechanics.

No proprietary executable or artwork is included in the repository or executed.
Set MM8_EXE_FIXTURE to an owned file when running outside the local port workspace.
"""
import csv
import hashlib
from io import BytesIO, StringIO
import json
import os
from pathlib import Path
import struct
import subprocess

from PIL import Image
import pytest
from test_menu_resources import lod, wrapped

BUILD = Path(__file__).parents[1] / "build-apple"
TOOL = Path(os.environ.get("ARCOMAGE_IMPORT_TOOL", BUILD / "arcomage_import"))
EXE = Path(os.environ.get("MM8_EXE_FIXTURE", "/Users/yukarii/Downloads/MM678_Merged_05.11.2023_gdrv/mm8.exe"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build arcomage_import first")


def write(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        csv.writer(stream, delimiter="\t", lineterminator="\n").writerows(rows)


def pcx(size, color):
    stream = BytesIO()
    Image.new("RGB", size, color).save(stream, "PCX")
    return stream.getvalue()


@pytest.fixture
def prepared(tmp_path):
    if not EXE.is_file():
        pytest.skip("Supply an owned mm8.exe via MM8_EXE_FIXTURE")
    root, base, output = (tmp_path / name for name in ("source", "base", "output"))
    (root / "Data").mkdir(parents=True)
    (root / "mm8.exe").write_bytes(EXE.read_bytes())
    lod(root / "Data/mm7.icons.lod", {"layout.pcx": wrapped("layout.pcx", pcx((640, 480), (1, 2, 3)))})
    lod(root / "Data/mm8.D.lod", {"sprites.pcx": wrapped("sprites.pcx", pcx((960, 1756), (4, 5, 6)))})
    lod(root / "Data/mmmerge.D.lod", {})
    tables = base / "engine/data_tables"
    write(tables / "map_stats.txt", [["1", "中文区域", "area.odm"] + ["0"] * 31 + ["mm6"]])
    write(tables / "house_data.txt", [["1", "1", "Tavern", "1", "", "酒馆", "店主", "", "1"] + ["0"] * 15])
    write(tables / "house_rules.txt", [["Index by type"], ["Arcomage in taverns"],
          ["1", "100", "200", "20", "10", "3", "3", "3", "15", "15", "15", "1", "100"]])
    write(tables / "transport_locations.txt", [["#", "Map"], ["0", "area.odm"] + ["-"] * 7 + ["0"] * 6])
    return root, base, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=30, umask=0o077)


def test_raw_binary_fields_mechanics_and_tavern_match(prepared):
    root, base, output = prepared
    before = {p: p.read_bytes() for folder in (root, base) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    rows = list(csv.reader(StringIO((output / "engine/data_tables/arcomage_cards.txt").read_text()), delimiter="\t"))
    assert rows[0] == ["#ArcomageCardMode", "MM8BinaryV1"] and len(rows) == 104
    binary = (root / "mm8.exe").read_bytes()
    names = {}
    for id_, row in enumerate(rows[2:]):
        data = binary[0xF0228 + 108 * id_:0xF0228 + 108 * (id_ + 1)]
        name, slot, *values = struct.unpack("<32sI8BI58b2x", data)
        assert row == [str(id_), name.split(b"\0")[0].decode(), str(slot), *map(str, values)]
        names[row[1]] = id_
    report = json.loads((output / "arcomage_conversion.json").read_text())
    assert report["cards"] == 102 and report["tavern_rules"] == 1 and report["typed_validation"]
    assert not report["runtime_ready"] and not report["executable_executed"] and not report["chinese_card_art"]
    assert not report["patched_windows_parity"] and not report["rendered_match_validated"]
    checks = json.loads((output / "arcomage_checks.json").read_text())
    plays = {(p["id"], p["variant"]): p for p in checks["plays"]}
    assert len(plays) == 306 and 0 < checks["tavern_matches"][0]["actions"] < 2000
    for variant in range(3):
        def after(name):
            return plays[names[name], variant]["after"]
        value = (variant + 1) * 10
        assert after("Brick Shortage")[0][5] == after("Brick Shortage")[1][5] == 92
        assert after("Lucky Cache")[0][5:7] == [102, 102]
        assert after("Amethyst")[0][0] == value + 3 and after("Amethyst")[0][6] == 98
        assert after("Werewolf")[1][:2] == [20, 11]
        assert after("Mother Lode")[0][2] == value + (2 if value < 20 else 1)
        assert after("Copping the Tech")[0][2] == max(value, 20)
        assert after("Parity")[0][3] == after("Parity")[1][3] == max(value, 20)
        player, enemy = after("Shift")
        assert player[1] == 20 and enemy[1] == value and player[5] == 83
        player, enemy = after("Flood Water")
        assert player[0] == value - (2 if value < 20 else 0)
        assert enemy[0] == 20 - (0 if value < 20 else 2)
        assert player[4] == value - (1 if value < 20 else 0)
        assert enemy[4] == 20 - (0 if value < 20 else 1)
        assert player[5] == 94
        assert after("Lightning Shard")[1][:2] == ([12, 20] if value > 20 else [20, 12])
    assert all(p.read_bytes() == data for p, data in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


def test_chinese_atlas_overrides_base_and_keeps_pcx_bytes(prepared):
    root, _, output = prepared
    original = pcx((960, 2500), (12, 34, 56))
    lod(root / "Data/10 LocZHCN.EnglishD.lod", {"sprites.pcx": wrapped("sprites.pcx", original)})
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    assert (output / "engine/icons/sprites.pcx").read_bytes() == original
    report = json.loads((output / "arcomage_conversion.json").read_text())
    assert report["chinese_card_art"]
    assert report["images"][0] == {"name": "sprites.pcx", "archive": "10 LocZHCN.EnglishD.lod",
                                  "width": 960, "height": 2500,
                                  "bgra_sha256": hashlib.sha256(bytes((56, 34, 12, 255)) * 960 * 2500).hexdigest()}
    assert Image.open(output / "engine/icons/layout.pcx").size == (640, 480)


def test_empty_localization_archive_does_not_claim_chinese_art(prepared):
    lod(prepared[0] / "Data/10 LocZHCN.EnglishD.lod", {})
    assert run(prepared).returncode == 0
    assert not json.loads((prepared[2] / "arcomage_conversion.json").read_text())["chinese_card_art"]


@pytest.mark.parametrize("fault", ["fingerprint", "truncated", "missing_exe", "missing_house_data", "missing_map",
                                    "no_tavern", "missing_rule", "missing_archive", "missing_layout", "short_atlas",
                                    "narrow_atlas", "wrong_background", "truncated_pixels"])
def test_invalid_sources_do_not_publish(prepared, fault):
    root, base, output = prepared
    if fault in ("fingerprint", "truncated"):
        data = bytearray((root / "mm8.exe").read_bytes())
        if fault == "fingerprint":
            data[0xF0228 + 48] ^= 1
        else:
            data = data[:0xF0228]
        (root / "mm8.exe").write_bytes(data)
    elif fault.startswith("missing_") and fault not in ("missing_layout", "missing_rule"):
        {"missing_exe": root / "mm8.exe", "missing_house_data": base / "engine/data_tables/house_data.txt",
         "missing_map": base / "engine/data_tables/map_stats.txt",
         "missing_archive": root / "Data/mm7.icons.lod"}[fault].unlink()
    elif fault == "no_tavern":
        write(base / "engine/data_tables/house_data.txt", [["1", "1", "House", "1", "", "房屋"]])
    elif fault == "missing_rule":
        write(base / "engine/data_tables/house_rules.txt", [["Index by type"], ["Unrelated"], ["1", "2"]])
    elif fault == "missing_layout":
        lod(root / "Data/mm7.icons.lod", {})
    elif fault == "wrong_background":
        lod(root / "Data/mm7.icons.lod", {"layout.pcx": wrapped("layout.pcx", pcx((32, 32), (0, 0, 0)))})
    elif fault == "truncated_pixels":
        lod(root / "Data/mm8.D.lod", {"sprites.pcx": wrapped("sprites.pcx", pcx((960, 2500), (0, 0, 0))[:128])})
    else:
        size = (960, 1755) if fault == "short_atlas" else (959, 2500)
        lod(root / "Data/mm8.D.lod", {"sprites.pcx": wrapped("sprites.pcx", pcx(size, (0, 0, 0)))})
    result = run(prepared)
    assert result.returncode != 0 and not output.exists(), result.stdout


@pytest.mark.parametrize("name", ["mm8.exe", "Data/mm8.D.lod", "Data/10 LocZHCN.EnglishD.lod"])
def test_input_symlink_cannot_escape_source(prepared, tmp_path, name):
    root, base, output = prepared
    path = root / name
    outside = tmp_path / "outside"
    if path.exists():
        path.rename(outside)
    else:
        lod(outside, {})
    path.symlink_to(outside)
    assert run(prepared).returncode != 0 and not output.exists()


def test_output_never_overwrites_inputs_or_existing_directory(prepared):
    root, base, output = prepared
    for parent in (root, base):
        assert run((root, base, parent / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"


def test_shared_binary_and_legacy_rules():
    tool = BUILD / "arcomage_binary_check"
    assert tool.is_file(), "Build arcomage_binary_check"
    result = subprocess.run([str(tool)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
