"""Global dynamic objects: animation directions, later palettes, source layers and rollback."""
import csv
import json
import os
from pathlib import Path
import struct
import subprocess
import zlib

from PIL import Image
import pytest

from test_decoration_resources import SFT_HEADER, frame
from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("OBJECT_PRESENTATION_IMPORT_TOOL",
                          Path(__file__).parents[1] / "build-apple/object_presentation_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build object_presentation_import first")


def write(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        csv.writer(stream, delimiter="\t").writerows(rows)


def sprite(name, value=1):
    header = bytearray(32)
    header[:len(name)] = name.encode()
    pixels = bytes([value] * 6)
    compressed = zlib.compress(pixels)
    struct.pack_into("<IHH", header, 12, len(compressed), 3, 2)
    struct.pack_into("<I", header, 28, len(pixels))
    return header + struct.pack("<hhIhhI", 0, 2, 0, 0, 2, 3) + compressed


def fixture(tmp_path):
    root, base = tmp_path / "source", tmp_path / "base"
    null = frame(0, "null", "null", palette=0)
    null[3], null[21], null[23] = "0", "0", "0"
    arrow = frame(1, "arrow", "bolt", palette=123)
    arrow[6] = "-"  # All eight viewing directions, not the single-image flag.
    rows = [SFT_HEADER, null, arrow, frame(2, "arrow", "bolt_b", start=False, palette=124),
            frame(3, "water", "splash", palette=123), frame(4, "unused", "branch", palette=125),
            frame(5, "", "unnamed", palette=126)]
    write(root / "Data/Tables/SFT.txt", rows)
    objects = [["Null", "null", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0"]]
    for descriptor, (name, group, sprite_id) in enumerate(
            [("Projectile", "arrow", 1), ("Water effect", "water", 3), ("Unnamed", "", 5)], 1):
        objects.append([name, group, str(500 + descriptor), "1", "1", "8", "10", str(sprite_id),
                        "0", "0", "0", "0", "4"])
    write(base / "engine/data_tables/object_list.txt", objects)
    images = {name: sprite(name) for name in [*["bolt" + str(i) for i in range(8)], "bolt_b", "splash",
                                             "branch", "unnamed"]}
    lod(root / "Data/sprites.lod", images, "MMVI")
    palettes = {"pal" + str(i): wrapped("pal" + str(i), b"\0", 1, 1,
                bytes([i % 256] * 768), name_size=16) for i in (123, 124, 125, 126)}
    lod(root / "Data/bitmaps.lod", palettes, "MMVI")
    return root, base, rows, objects, images, palettes


def run(root, base, output):
    return subprocess.run([str(TOOL), str(root), str(base), str(output)], text=True,
                          capture_output=True, umask=0o077)


def test_all_dynamic_groups_directions_and_later_frame_palettes(tmp_path):
    root, base, _, _, _, _ = fixture(tmp_path)
    # The named Merge layer wins regardless of directory enumeration order.
    lod(root / "Data/mmmerge.sprites.lod", {"bolt_b": sprite("bolt_b", 7)}, "MMVI")
    output = tmp_path / "converted"
    result = run(root, base, output)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / "object_presentation_conversion.json").read_text())
    assert report["object_descriptors"] == 4 and report["sprite_group_ids"] == [1, 3, 5]
    assert report["sentinel_descriptors"] == [0] and report["runtime_ready"] is False
    assert report["missing_sprites"] == report["missing_palettes"] == []
    expected = {*("bolt" + str(i) + ".bmp" for i in range(8)), "bolt_b.bmp", "splash.bmp", "unnamed.bmp"}
    assert {p.name for p in (output / "engine/sprites").iterdir()} == expected
    for name in expected:
        path = output / "engine/sprites" / name
        image = Image.open(path)
        assert image.size == (3, 2)
        assert image.convert("L").tobytes() == bytes([7 if name == "bolt_b.bmp" else 1] * 6)
        assert path.stat().st_mode & 0o777 == 0o644
    assert {p.name for p in (output / "engine/textures").iterdir()} == {"pal123.act", "pal124.act", "pal126.act"}
    assert (output / "engine/textures/pal124.act").read_bytes() == bytes([124] * 768)
    assert next(r for r in report["images"] if r["name"] == "bolt_b.bmp")["archive"] == "mmmerge.sprites.lod"


def test_missing_resources_are_reported_without_replacement(tmp_path):
    root, base, _, _, images, palettes = fixture(tmp_path)
    del images["bolt7"]
    del palettes["pal124"]
    lod(root / "Data/sprites.lod", images, "MMVI")
    lod(root / "Data/bitmaps.lod", palettes, "MMVI")
    output = tmp_path / "converted"
    result = run(root, base, output)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / "object_presentation_conversion.json").read_text())
    assert report["missing_sprites"] == ["bolt7"] and report["missing_palettes"] == ["pal124"]
    assert not (output / "engine/sprites/bolt7.bmp").exists()
    assert not (output / "engine/textures/pal124.act").exists()


@pytest.mark.parametrize("fault", ["frame_id", "unused_bad_flag", "object_group_id", "object_group_name",
                                    "corrupt_sprite", "corrupt_palette", "archive_escape"])
def test_faults_preserve_sources_and_remove_partial_output(tmp_path, fault):
    root, base, rows, objects, images, palettes = fixture(tmp_path)
    if fault == "frame_id":
        rows[3][0] = "100"
    elif fault == "unused_bad_flag":
        rows[5][8] = "maybe"
    elif fault == "object_group_id":
        objects[1][7] = "2"
    elif fault == "object_group_name":
        objects[1][1] = "water"
    elif fault == "corrupt_sprite":
        images["splash"] = b"broken"
    elif fault == "corrupt_palette":
        palettes["pal126"] = b"broken"
    write(root / "Data/Tables/SFT.txt", rows)
    write(base / "engine/data_tables/object_list.txt", objects)
    lod(root / "Data/sprites.lod", images, "MMVI")
    lod(root / "Data/bitmaps.lod", palettes, "MMVI")
    if fault == "archive_escape":
        archive = root / "Data/sprites.lod"
        outside = tmp_path / "outside.lod"
        archive.rename(outside)
        archive.symlink_to(outside)
    before = {p: p.read_bytes() for directory in (root, base) for p in directory.rglob("*") if p.is_file()}
    output = tmp_path / "converted"
    result = run(root, base, output)
    messages = {"frame_id": "SFT index/width", "unused_bad_flag": "Unknown SFT flag",
                "object_group_id": "inconsistent SFT group", "object_group_name": "SFT name differs",
                "corrupt_sprite": "Truncated legacy resource header",
                "corrupt_palette": "Truncated legacy resource header", "archive_escape": "escapes source"}
    assert result.returncode != 0 and messages[fault] in result.stderr
    assert not output.exists()
    assert all(p.read_bytes() == data for p, data in before.items())


def test_existing_and_overlapping_destinations_are_protected(tmp_path):
    root, base, *_ = fixture(tmp_path)
    assert run(root, base, root / "nested").returncode != 0
    assert run(root, base, base / "nested").returncode != 0
    output = tmp_path / "existing"
    output.mkdir()
    (output / "keep").write_bytes(b"untouched")
    assert run(root, base, output).returncode != 0
    assert (output / "keep").read_bytes() == b"untouched"
