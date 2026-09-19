"""Synthetic sparse sprite scanlines and palette archives; no original game fixtures."""
import os
from pathlib import Path
import struct
import subprocess
import zlib

from PIL import Image
import pytest
from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("SPRITE_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/sprite_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build sprite_import first")


def source(root, compressed=True, fault=None):
    root.mkdir()
    header = bytearray(32)
    header[:5] = b"actor"
    pixels = bytes((1, 2, 3, 4, 5))
    stored = zlib.compress(pixels) if compressed else pixels
    struct.pack_into("<IHH", header, 12, len(stored), 5, 3)
    struct.pack_into("<I", header, 28, len(pixels) if compressed else 0)
    rows = bytearray(struct.pack("<hhIhhIhhI", 1, 3, 0, -1, -1, 0, 0, 1, 3))
    if fault == "bounds":
        struct.pack_into("<h", rows, 2, 6)
    elif fault == "offset":
        struct.pack_into("<I", rows, 4, 20)
    elif fault == "negative":
        struct.pack_into("<h", rows, 0, -2)
    elif fault == "compressed":
        stored = bytes([stored[0] ^ 255]) + stored[1:]
    lod(root / "sprites.lod", {"actor": header + rows + stored}, "MMVI")
    palette = bytes((i % 256 for i in range(768)))
    entries = {} if fault == "palette" else {"pal123": wrapped("pal123", b"\0", 1, 1, palette, name_size=16)}
    lod(root / "bitmaps.lod", entries, "MMVI")
    return palette


def run(root, output):
    return subprocess.run([str(TOOL), str(root), "actor", "123", str(output)], capture_output=True, text=True)


@pytest.mark.parametrize("compressed", [True, False])
def test_sparse_rows_indices_orientation_and_palette(tmp_path, compressed):
    root = tmp_path / "source"
    palette = source(root, compressed)
    output = tmp_path / "output"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    im = Image.open(output / "actor.bmp")
    # Pillow optimizes an identity grayscale palette to L; the file remains indexed BMP.
    assert im.mode in ("P", "L") and im.size == (5, 3)
    assert struct.unpack_from("<H", (output / "actor.bmp").read_bytes(), 28)[0] == 8
    assert [im.getpixel((x, y)) for y in range(3) for x in range(5)] == [
        0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 4, 5, 0, 0, 0]
    assert (output / "pal123.act").read_bytes() == palette


@pytest.mark.parametrize("fault", ["bounds", "offset", "negative", "compressed", "palette"])
def test_bad_sprite_rolls_back_and_preserves_original(tmp_path, fault):
    root = tmp_path / "source"
    source(root, fault=fault)
    before = {p: p.read_bytes() for p in root.iterdir()}
    output = tmp_path / "output"
    assert run(root, output).returncode != 0
    assert not output.exists()
    assert all(p.read_bytes() == original for p, original in before.items())


def test_rejects_overlap_and_existing_output(tmp_path):
    root = tmp_path / "source"
    source(root)
    assert run(root, root / "inside").returncode != 0
    existing = tmp_path / "existing"
    existing.mkdir()
    marker = existing / "marker"
    marker.write_text("preserve")
    assert run(root, existing).returncode != 0
    assert marker.read_text() == "preserve"
