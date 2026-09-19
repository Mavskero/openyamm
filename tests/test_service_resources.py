"""Native chest/house import: coordinate spaces, localization, ownership and rollback."""
import json
import os
from pathlib import Path
import stat
import subprocess

from PIL import Image
import pytest

from test_core_resources import archive, rows, text
from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("SERVICE_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/service_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build service_import first")


def source(root, chinese=True, pictures=True, fault=None):
    data = root / "Data"
    (data / "Tables").mkdir(parents=True)
    chests = [["#", "Name", "Width", "Height", "ImageIndex"]]
    chests += [[str(i), f"Chest {i}", "14", "10", str(i + 1)] for i in range(8)]
    if fault == "chest_id":
        chests[3][0] = "8"
    if fault == "image_id":
        chests[2][4] = "8"
    if fault == "storage_size":
        chests[1][2] = "256"
    if fault == "chest_columns":
        chests[1].pop()
    (data / "Tables/Chest.txt").write_text(text(chests))
    header = ["#", "#", "Type", "Map", "Picture", "Name"] + [""] * 18
    houses = [["2D Events by Type"] + [""] * 23, header]
    for id_, name in [(1, "Forge"), (3, "Temple"), (5, "")]:
        row = [str(id_), "1", "Weapon Shop", "151", "16", name, "Owner", "Blacksmith", "1129"] + ["0"] * 15
        row[12:14] = ["1.5", "2.5"]
        row[15], row[18], row[19], row[23] = "7", "6", "18", "Welcome"
        houses.append(row)
    if fault == "duplicate_house":
        houses.append(houses[2])
    if fault == "short_house":
        houses[2] = houses[2][:5]
    archive(data / "mmmerge.T.lod", {"2devents.txt": houses})
    if chinese:
        archive(data / "10 LocZHCN.EnglishT.lod", {"marker.txt": [["1", "中文"]]}, True)
        translated = [["#", "Name", "Proprieter Name", "Title", "Enter text"],
                      ["1", "铁匠铺", "老王", "铁匠", '他说："欢迎"\n再见'],
                      ["3", "Temple", "小李", "店主", "欢迎"],
                      ["5", "不可激活的店铺", "", "", ""]]
        if fault == "blank_translation":
            translated[1][1] = ""
        if fault == "duplicate_translation":
            translated.append(translated[1])
        if fault == "locale_coverage":
            del translated[2]
            translated.append(["99", "旧版房屋", "", "", ""])
        (data / "Text localization").mkdir()
        (data / "Text localization/ZHCN_2DEvents.txt").write_bytes(text(translated).encode("gb2312"))
    if pictures:
        palette = bytes((0, 0, 0, 255, 0, 0)) + bytes(762)
        entries = {f"chest0{i}": wrapped(f"chest0{i}", b"\1" * (460 * 344), 460, 344, palette)
                   for i in range(1, 9)}
        if fault == "small_background":
            entries["chest01"] = wrapped("chest01", b"\1" * 100, 10, 10, palette)
        if fault == "broken_background":
            entries["chest01"] = entries["chest01"][:-5]
        lod(data / "mm8.D.lod", entries)
        if chinese and not fault:
            palette = bytes((0, 0, 0, 0, 0, 255)) + bytes(762)
            lod(data / "10 LocZHCN.EnglishD.lod",
                {"CHEST01": wrapped("CHEST01", b"\1" * (460 * 344), 460, 344, palette)})
    return root


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True, umask=0o077)


@pytest.mark.parametrize("chinese", [True, False])
def test_native_services_preserve_coordinates_ids_and_mechanics(tmp_path, chinese):
    root = source(tmp_path / "source", chinese)
    originals = {p: p.read_bytes() for p in root.rglob("*") if p.is_file()}
    output = tmp_path / "services"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    chests = rows(output / "engine/data_tables/chest_data.txt")
    assert len(chests) == 8 and [int(row[0]) for row in chests] == list(range(8))
    assert all(row[2:4] == ["14", "10"] and row[8:10] == ["9", "9"] for row in chests)
    assert chests[0][6:8] == ["34", "26"]  # Screen (42, 49) minus background origin (8, 23).
    assert chests[1][6:8] == ["10", "22"]  # Different chest layout, same 32-pixel cells.
    houses = {row[0]: row for row in rows(output / "engine/data_tables/house_data.txt")}
    assert set(houses) == {"1", "3"}  # Missing IDs remain missing; inactive translation creates no venue.
    assert houses["1"][5] == ("铁匠铺" if chinese else "Forge")
    assert houses["3"][2] == "Weapon Shop" and houses["3"][5] == "Temple"
    assert houses["1"][12:14] == ["1.5", "2.5"] and houses["1"][18:20] == ["6", "18"]
    assert houses["1"][23] == ('他说："欢迎"\n再见' if chinese else "Welcome")
    manifest = json.loads((output / "service_conversion.json").read_text())
    assert manifest["typed_validation"] and not manifest["runtime_ready"]
    assert manifest["inactive_house_ids"] == [5] and manifest["translated_houses"] == (2 if chinese else 0)
    assert manifest["ignored_inactive_translation_ids"] == ([5] if chinese else [])
    assert manifest["missing_chest_images"] == [] and len(manifest["chest_images"]) == 8
    image = Image.open(output / "engine/icons/chest01.bmp")
    assert image.size == (460, 344) and image.getpixel((0, 0)) == ((0, 0, 255) if chinese else (255, 0, 0))
    assert all(p.read_bytes() == data for p, data in originals.items())
    assert all(stat.S_IMODE(p.stat().st_mode) == 0o644 for p in output.rglob("*") if p.is_file())


def test_missing_mm8_backgrounds_are_explicit_without_substituting_mm6(tmp_path):
    root = source(tmp_path / "source", pictures=False)
    lod(root / "Data/mm6.icons.lod", {"6chest01": b"world-specific image"})
    output = tmp_path / "services"
    assert run(root, output).returncode == 0
    manifest = json.loads((output / "service_conversion.json").read_text())
    assert manifest["missing_chest_images"] == [f"chest0{i}" for i in range(1, 9)]
    assert not manifest["interactive_chests_ready"] and not (output / "engine/icons").exists()


def test_house_locale_version_differences_are_reported(tmp_path):
    root = source(tmp_path / "source", fault="locale_coverage")
    output = tmp_path / "services"
    assert run(root, output).returncode == 0
    manifest = json.loads((output / "service_conversion.json").read_text())
    assert manifest["extra_house_translation_ids"] == [99]
    assert manifest["missing_house_translation_ids"] == [3]
    assert manifest["translated_houses"] == 1


@pytest.mark.parametrize("fault", ["chest_id", "image_id", "storage_size", "chest_columns", "duplicate_house",
                                   "short_house", "blank_translation", "duplicate_translation",
                                   "small_background", "broken_background"])
def test_invalid_services_or_images_do_not_publish_partial_output(tmp_path, fault):
    root = source(tmp_path / "source", fault=fault)
    output = tmp_path / "services"
    result = run(root, output)
    assert result.returncode != 0 and "Service conversion failed" in result.stderr
    assert not output.exists()


def test_source_symlink_escape_and_output_preservation(tmp_path):
    root = source(tmp_path / "source")
    output = tmp_path / "services"
    existing = tmp_path / "existing"
    existing.mkdir()
    (existing / "keep").write_text("preserve")
    assert run(root, existing).returncode != 0
    assert (existing / "keep").read_text() == "preserve"
    assert run(root, root / "converted").returncode != 0 and not (root / "converted").exists()
    original = root / "Data/mmmerge.T.lod"
    outside = tmp_path / "outside.lod"
    original.rename(outside)
    original.symlink_to(outside)
    assert run(root, output).returncode != 0 and not output.exists()
