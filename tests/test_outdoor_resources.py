"""Native scene conversion against small synthetic archives, no original game fixtures."""
import json
import os
from pathlib import Path
import struct
import subprocess
import zlib

from PIL import Image
import pytest
from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("OUTDOOR_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/outdoor_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build outdoor_import first")
HEADER = "# Name Id TileSet Section Burn Water Block Repulse Flat Wave NoDraw WaterTransition Transition " \
         "ScrollDown ScrollUp ScrollLeft ScrollRight"


def source(root):
    data = root / "Data"
    (data / "Tables").mkdir(parents=True)
    geometry = bytearray(0xb4 + 0x4000 * 3 + 4 + 0x20000 + 0x10000 + 12 + 0x4000 * 4 + 4)
    geometry[0x5f] = 2
    geometry[0xb4 + 0x4000:0xb4 + 0x8000] = bytes([1]) * 0x4000
    for i, base in enumerate((90, 126, 162, 198)):
        struct.pack_into("<I", geometry, 0xa0 + i * 4, base << 16)
    compressed = zlib.compress(geometry)
    payload = b"\0\0\0\0mvii" + struct.pack("<II", len(compressed), len(geometry)) + compressed
    lod(data / "mm6.games.lod", {"oute3.odm": payload}, "GameMMVI")
    rows = [HEADER.replace(" ", "\t")]
    for i in range(234):
        flags = ["x", "x"] + ["-"] * 5 + ["x", "-"] + ["-"] * 4
        rows.append("\t".join([str(i), "dirt" if i == 1 else "pending", "0", "4", "0"] + flags))
    (data / "Tables/Tile3.txt").write_text("\r\n".join(rows) + "\r\n")
    palette = bytearray(768)
    palette[3:6] = bytes((23, 45, 67))
    # Four levels: 8x8 + 4x4 + 2x2 + 1x1. Only the base level becomes the BMP.
    pixels = bytes([1]) * 64 + bytes([2]) * 21
    lod(data / "mm6.bitmaps.lod", {name: wrapped(name, pixels, 8, 8, palette, name_size=16)
                                 for name in ("dirt", "6sky05")}, "MMVI")
    for name in ("mm7.bitmaps.lod", "mm8.bitmaps.lod", "mmmerge.bitmaps.lod", "00 patch.bitmaps.lod"):
        lod(data / name, {}, "MMVI")
    return root, bytes(geometry)


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], text=True, capture_output=True)


def test_native_map_mipmaps_and_terrain_flags(tmp_path):
    root, geometry = source(tmp_path / "source")
    output = tmp_path / "output"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    assert (output / "worlds/mm6/maps/oute3.odm").read_bytes() == geometry
    image = Image.open(output / "worlds/mm6/textures/dirt.bmp")
    assert image.size == (8, 8) and image.getpixel((0, 0)) == (23, 45, 67)
    rows = (output / "engine/data_tables/terrain_tile_data_3.txt").read_text().splitlines()
    assert rows[2].split("\t") == ["1", "dirt", "0", "0", "4", "0", "259"]
    manifest = json.loads((output / "conversion.json").read_text())
    assert manifest["runtime_ready"] is False and manifest["terrain_rows"] == 234
    assert manifest["geometry_archive"] == "mm6.games.lod"


@pytest.mark.parametrize("fault", ["row_id", "flag", "header", "missing_texture", "map_wrapper"])
def test_scene_fault_preserves_source_and_does_not_publish_partial_output(tmp_path, fault):
    root, _ = source(tmp_path / "source")
    table = root / "Data/Tables/Tile3.txt"
    text = table.read_text()
    if fault == "row_id":
        table.write_text(text.replace("\n1\tdirt", "\n7\tdirt", 1))
    elif fault == "flag":
        table.write_text(text.replace("\tx\t", "\tmaybe\t", 1))
    elif fault == "header":
        table.write_text(text.replace("WaterTransition", "Unknown", 1))
    elif fault == "missing_texture":
        table.write_text(text.replace("\tdirt\t", "\tmissing\t", 1))
    elif fault == "map_wrapper":
        lod(root / "Data/mm6.games.lod", {"oute3.odm": b"broken"}, "GameMMVI")
    before = {str(p): p.read_bytes() for p in root.rglob("*") if p.is_file()}
    output = tmp_path / "output"
    assert run(root, output).returncode != 0
    assert not output.exists()
    assert all(Path(p).read_bytes() == content for p, content in before.items())


def test_scene_rejects_overlapping_and_existing_destination(tmp_path):
    root, _ = source(tmp_path / "source")
    assert run(root, root / "converted").returncode != 0
    existing = tmp_path / "keep"
    existing.mkdir()
    (existing / "marker").write_text("preserve")
    assert run(root, existing).returncode != 0
    assert (existing / "marker").read_text() == "preserve"


def test_missing_actor_data_rolls_back_entire_scene(tmp_path):
    root, _ = source(tmp_path / "source")
    output = tmp_path / "output"
    result = subprocess.run([str(TOOL), str(root), str(output), "--actors"], capture_output=True)
    assert result.returncode != 0
    assert not output.exists()
