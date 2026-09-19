"""Validate the native iPad menu converter with synthetic LODs, not copyrighted fixtures."""
from io import BytesIO
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import zlib

from PIL import Image
import pytest

TOOL = Path(os.environ.get("MENU_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/menu_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build menu_import first")
NAMES = [f"t_{button}_{state}" for button in ("new", "load", "cred", "quit") for state in ("up", "ht", "dn")]


def wrapped(name, data, width=0, height=0, palette=b"", compressed=True, name_size=64):
    stored = zlib.compress(data) if compressed else data
    header = bytearray(name_size + 32)
    header[:len(name)] = name.encode("ascii")
    struct.pack_into("<IIHH", header, name_size, width * height, len(stored), width, height)
    struct.pack_into("<I", header, name_size + 24, len(data) if compressed else 0)
    return bytes(header) + stored + palette


def lod(path, entries, version="MMVIII"):
    name_size, stride = (64, 76) if version == "MMVIII" else (16, 32)
    header = bytearray(288)
    header[:4] = b"LOD\0"
    header[4:4 + len(version)] = version.encode("ascii")
    index = bytearray(len(entries) * stride)
    data = bytearray()
    for i, (name, payload) in enumerate(entries.items()):
        start = i * stride
        index[start:start + len(name)] = name.encode("ascii")
        struct.pack_into("<II", index, start + name_size, len(index) + len(data), len(payload))
        data.extend(payload)
    struct.pack_into("<IIII", header, 272, 288, len(index) + len(data), 0, len(entries))
    path.write_bytes(header + index + data)


def source(root, compressed=True, legacy=False):
    data = root / "Data"
    data.mkdir(parents=True)
    palette = bytearray(768)
    palette[3:6] = bytes((255, 0, 0))
    palette[6:9] = bytes((0, 255, 0))
    pixels = bytes((1, 2, 2, 1, 2, 1))  # 3x2, exercises BMP row padding and top/bottom order.
    width = 16 if legacy else 64
    entries = {name: wrapped(name, pixels, 3, 2, palette, compressed, width) for name in NAMES}
    pcx = BytesIO()
    Image.new("RGB", (4, 3), (12, 34, 56)).save(pcx, "PCX")
    entries["title.pcx"] = wrapped("title.pcx", pcx.getvalue(), compressed=compressed, name_size=width)
    lod(data / "mm8.D.lod", entries, "MMVI" if legacy else "MMVIII")
    lod(data / "mmmerge.D.lod", {})
    return root, entries, pcx.getvalue()


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True, umask=0o077)


@pytest.mark.parametrize("compressed,legacy", [(True, False), (False, False), (True, True)])
def test_palette_orientation_padding_pcx_and_incomplete_manifest(tmp_path, compressed, legacy):
    root, _, pcx = source(tmp_path / "source", compressed, legacy)
    output = tmp_path / "preview"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    image = Image.open(output / "engine/icons/t_new_up.bmp").convert("RGB")
    assert image.size == (3, 2)
    assert [image.getpixel((x, y)) for y in range(2) for x in range(3)] == [
                                    (255, 0, 0), (0, 255, 0), (0, 255, 0),
                                    (255, 0, 0), (0, 255, 0), (255, 0, 0)]
    assert (output / "engine/icons/title.pcx").read_bytes() == pcx
    manifest = json.loads((output / "conversion.json").read_text())
    assert manifest["runtime_ready"] is False
    assert len(manifest["assets"]) == 13
    assert all(stat.S_IMODE(path.stat().st_mode) == 0o644 for path in output.rglob("*") if path.is_file())


def test_chinese_overlay_wins_case_insensitively_and_records_owner(tmp_path):
    root, entries, _ = source(tmp_path / "source")
    # A localized button with a distinct palette proves priority, not just file presence.
    palette = bytearray(768)
    palette[3:6] = bytes((0, 0, 255))
    name = "T_NEW_UP"
    lod(root / "Data/10 LocZHCN.EnglishD.lod", {name: wrapped(name, b"\1", 1, 1, palette)})
    output = tmp_path / "preview"
    assert run(root, output).returncode == 0
    assert Image.open(output / "engine/icons/t_new_up.bmp").getpixel((0, 0)) == (0, 0, 255)
    assets = json.loads((output / "conversion.json").read_text())["assets"]
    assert next(a for a in assets if a["name"] == "t_new_up.bmp")["archive"] == "10 LocZHCN.EnglishD.lod"


@pytest.mark.parametrize("fault", ["compressed", "palette", "missing", "duplicate", "overlap"])
def test_bad_input_does_not_publish_partial_output(tmp_path, fault):
    root, entries, _ = source(tmp_path / "source")
    if fault == "compressed":
        damaged = bytearray(entries["t_new_up"])
        damaged[96] ^= 255
        entries["t_new_up"] = bytes(damaged)
    elif fault == "palette":
        entries["t_new_up"] = entries["t_new_up"][:-1]
    elif fault == "missing":
        del entries["t_quit_dn"]
    elif fault == "duplicate":
        entries["T_NEW_UP"] = entries["t_new_up"]
    lod(root / "Data/mm8.D.lod", entries)
    archive = root / "Data/mm8.D.lod"
    if fault == "overlap":
        damaged = bytearray(archive.read_bytes())
        first_offset = struct.unpack_from("<I", damaged, 288 + 64)[0]
        struct.pack_into("<I", damaged, 288 + 76 + 64, first_offset)
        archive.write_bytes(damaged)
    original = archive.read_bytes()
    output = tmp_path / "preview"
    assert run(root, output).returncode != 0
    assert not output.exists()
    assert archive.read_bytes() == original


def test_refuses_existing_output_and_source_descendant(tmp_path):
    root, _, _ = source(tmp_path / "source")
    existing = tmp_path / "existing"
    existing.mkdir()
    marker = existing / "keep.txt"
    marker.write_text("unchanged")
    assert run(root, existing).returncode != 0
    assert marker.read_text() == "unchanged"
    assert run(root, root / "converted").returncode != 0
    assert not (root / "converted").exists()
