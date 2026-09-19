"""Original decoration IDs, global animation frames and map-owned art integration."""
import csv
import json
import os
from pathlib import Path
import struct
import subprocess
import zlib

from PIL import Image
import pytest
import yaml

from test_menu_resources import lod, wrapped
from test_outdoor_resources import source as outdoor_source
from test_sprite_resources import source as sprite_source

TOOL = Path(os.environ.get("DECORATION_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/decoration_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build decoration_import first")
DEC_HEADER = ["#", "Name", "GameName", "Type", "Height", "Radius", "LightRadius", "SFTGroup",
              "NoBlockMovement", "NoDraw", "FlickerSlow", "FlickerMedium", "FlickerFast", "Marker", "SlowLoop",
              "EmitFire", "SoundOnDawn", "SoundOnDusk", "EmitSmoke", "SoundId", "Red", "Green", "Blue"]
SFT_HEADER = ["#", "GroupName", "SpriteName", "Scale", "Luminous", "GroupStart", "Image1", "Center", "Fidget",
              "Loaded", "Mirror0", "Mirror1", "Mirror2", "Mirror3", "Mirror4", "Mirror5", "Mirror6", "Mirror7",
              "Images3", "Glow", "Transparent", "LightRadius", "PaletteId", "Time"]


def write(path, rows):
    with path.open("w", newline="") as stream:
        csv.writer(stream, delimiter="\t").writerows(rows)


def read(path):
    with path.open(newline="") as stream:
        return list(csv.reader(stream, delimiter="\t"))


def frame(index, group, texture, start=True, palette=123):
    flags = ["-"] * 17
    flags[1] = "x" if start else ""  # Original table accepts blank false GroupStart cells.
    flags[2] = "x"
    return [str(index), group, texture, "98304", *flags, "7", str(palette), "2"]


def set_map(root, geometry, direction=512, degrees=0, tree_name="TrEe"):
    offset = 0xb4 + 0x4000 * 3 + 4 + 0x20000 + 0x10000 + 4
    records = bytearray(64)
    # Cached IDs are deliberately wrong: the named merged registry must win.
    struct.pack_into("<HHiiiiHHHH", records, 0, 2, 0, -9728, -11319, 160, direction, 0, 0, 0, degrees)
    struct.pack_into("<HHiiii", records, 32, 1, 0, -9000, -11000, 160, 1024)
    names = b"Party Start".ljust(32, b"\0") + tree_name.encode().ljust(32, b"\0")
    result = geometry[:offset] + struct.pack("<I", 2) + records + names + geometry[offset + 4:]
    compressed = zlib.compress(result)
    payload = b"\0\0\0\0mvii" + struct.pack("<II", len(compressed), len(result)) + compressed
    lod(root / "Data/mm6.games.lod", {"oute3.odm": payload}, "GameMMVI")


@pytest.fixture
def prepared(tmp_path):
    root, geometry = outdoor_source(tmp_path / "source")
    data = root / "Data"
    rows = [DEC_HEADER,
            ["0", "null", "null", "0", "8", "8", "0", "null", *(["-"] * 11), "0", "255", "255", "255"],
            ["1", "Party Start", "null", "0", "8", "8", "0", "null",
             "-", "x", *(["-"] * 9), "0", "255", "255", "255"],
            ["2", "Tree", "Ancient tree", "0", "300", "31", "32", "tree",
             "-", "-", "x", "x", "x", "x", "x", "x", "x", "x", "x", "42695", "12", "34", "56"]]
    write(data / "Tables/DecList.txt", rows)
    write(data / "Tables/SFT.txt", [SFT_HEADER, frame(0, "null", "pending", palette=0),
          frame(1, "", "pending", palette=0), frame(2, "tree", "actor"),
          frame(3, "", "actor", start=False), frame(4, "unused", "elsewhere"),
          frame(5, "", "pending", palette=0)])
    sprite_root = tmp_path / "sprite"
    palette = sprite_source(sprite_root)
    (data / "mm6.sprites.lod").write_bytes((sprite_root / "sprites.lod").read_bytes())
    (data / "mmmerge.bitmaps.lod").write_bytes((sprite_root / "bitmaps.lod").read_bytes())
    set_map(root, geometry)
    return root, tmp_path / "converted", geometry, palette


def run(prepared):
    return subprocess.run([str(TOOL), str(prepared[0]), str(prepared[1])],
                          capture_output=True, text=True, timeout=20, umask=0o077)


def report(prepared):
    return json.loads((prepared[1] / "decoration_conversion.json").read_text())


def test_global_registry_animation_source_ids_and_actual_art(prepared):
    root, output, _, palette = prepared
    before = {p: p.read_bytes() for p in root.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    rows = read(output / "engine/data_tables/decoration_data.txt")
    assert rows[3] == ["3", "tree", "Ancient tree", "0", "31", "300", "32", "12", "34", "56",
                       "-22841", "2044", "2", "tree", "", "42695"]
    assert [r[0] for r in rows[1:]] == ["1", "2", "3"]
    frames = yaml.safe_load((output / "engine/rendering/sprite_frame_data_common.yml").read_text())["sprites"]
    assert [g["sprite_id"] for g in frames] == [0, 1, 2, 4, 5]
    assert frames[2]["animation_length_raw"] == 4 and len(frames[2]["frames"]) == 2
    assert frames[2]["frames"][0]["flags"] == 21
    assert frames[2]["frames"][1]["flags"] == 16
    assert frames[2]["frames"][0]["scale"] == 1.5
    assert frames[3]["sprite_name"] == "unused"  # Full SFT must retain other actor/object groups.
    image = Image.open(output / "worlds/mm6/sprites/actor.bmp")
    assert [image.getpixel((x, y)) for y in range(3) for x in range(5)] == [
        0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 4, 5, 0, 0, 0]
    assert (output / "worlds/mm6/textures/pal123.act").read_bytes() == palette
    manifest = report(prepared)
    assert manifest["named_resolutions"] == manifest["map_entities"] == 2
    assert manifest["sft_frames"] == 6 and manifest["sft_groups"] == 5
    assert manifest["sprites_exported"] == manifest["palettes_exported"] == 1
    assert manifest["signed_sound_storage_rows"] == 1
    assert manifest["missing_sprites"] == manifest["missing_palettes"] == manifest["unresolved_entity_names"] == []
    assert manifest["party_starts"] == [{"entity_index": 0, "x": -9728, "y": -11319, "z": 160,
                                        "direction_yaw_units": 512, "facing_degrees": 90}]
    assert not manifest["runtime_ready"] and not manifest["ambient_audio_validated"]
    assert (output / "import_sources/declist.txt").read_bytes() == before[root / "Data/Tables/DecList.txt"]
    assert all(p.read_bytes() == content for p, content in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("direction,degrees,expected", [(0, 90, 90), (0, 65446, 270), (-512, 90, 270),
                                                        (2048, 90, 0), (1, 0, 360 / 2048)])
def test_legacy_direction_units_and_zero_direction_degree_fallback(prepared, direction, degrees, expected):
    set_map(prepared[0], prepared[2], direction, degrees)
    assert run(prepared).returncode == 0
    assert report(prepared)["party_starts"][0]["facing_degrees"] == pytest.approx(expected, abs=0.000001)


@pytest.mark.parametrize("fault", ["index", "duplicate", "flag", "sound", "color", "group",
                                    "sft_start", "sft_duplicate", "sft_index", "sft_flag", "sft_texture"])
def test_invalid_registry_or_frames_do_not_publish(prepared, fault):
    root, output, _, _ = prepared
    filename, row, col, value = {
        "index": ("DecList.txt", 2, 0, "8"), "duplicate": ("DecList.txt", 3, 1, "PARTY START"),
        "flag": ("DecList.txt", 3, 8, "maybe"), "sound": ("DecList.txt", 3, 19, "65536"),
        "color": ("DecList.txt", 3, 20, "256"), "group": ("DecList.txt", 3, 7, "absent"),
        "sft_start": ("SFT.txt", 1, 5, "-"), "sft_duplicate": ("SFT.txt", 5, 1, "TREE"),
        "sft_index": ("SFT.txt", 4, 0, "9"), "sft_flag": ("SFT.txt", 3, 4, "maybe"),
        "sft_texture": ("SFT.txt", 3, 2, "../actor"),
    }[fault]
    path = root / "Data/Tables" / filename
    rows = read(path)
    rows[row][col] = value
    write(path, rows)
    assert run(prepared).returncode != 0 and not output.exists()


def test_missing_art_is_explicit_and_existing_corrupt_art_rolls_back(prepared):
    root, output, _, _ = prepared
    lod(root / "Data/mm6.sprites.lod", {}, "MMVI")
    assert run(prepared).returncode == 0
    assert report(prepared)["missing_sprites"] == ["actor"]
    assert report(prepared)["sprites_exported"] == 0
    lod(root / "Data/mm6.sprites.lod", {"actor": b"corrupt"}, "MMVI")
    failed = (root, output.with_name("bad-art"), None, None)
    assert run(failed).returncode != 0 and not failed[1].exists()


def test_highest_archive_priority_and_missing_palette(prepared):
    root, output, _, _ = prepared
    (root / "Data/mmmerge.sprites.lod").write_bytes((root / "Data/mm6.sprites.lod").read_bytes())
    lod(root / "Data/mmmerge.bitmaps.lod", {}, "MMVI")
    assert run(prepared).returncode == 0
    manifest = report(prepared)
    assert manifest["sprite_owners"][0]["archive"] == "mmmerge.sprites.lod"
    assert manifest["missing_palettes"] == [123]


def test_existing_overlap_and_symlink_inputs_preserved(prepared, tmp_path):
    root, output, _, _ = prepared
    assert run((root, root / "inside", None, None)).returncode != 0
    output.mkdir()
    (output / "keep").write_text("preserve")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "preserve"
    target = root / "Data/Tables/DecList.txt"
    outside = tmp_path / "outside.txt"
    target.rename(outside)
    target.symlink_to(outside)
    failed = (root, output.with_name("escaped"), None, None)
    assert run(failed).returncode != 0 and not failed[1].exists()
