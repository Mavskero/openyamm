"""Item-table-driven image import, exact pixels, source ownership and rollback."""
import json
import struct

from PIL import Image
import pytest

from test_inventory_resources import source, run, TOOL
from test_menu_resources import lod, wrapped

pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build inventory_import first")


def bitmap(name, color):
    palette = bytearray(768)
    palette[3:6] = bytes(color)
    return wrapped(name, bytes([0, 1, 1, 0, 1, 1]), 3, 2, palette, name_size=16)


def fixture(root, assets):
    def mutate(base, locale, settings, items, notes):
        for rows in [base["items.txt"], items]:
            for row in rows:
                row[1] = "Cndlabra" if row[0] == "700" else "herb03" if row[0] == "773" else "absent"
    source(root, assets, mutate=mutate)
    lod(root / "Data/mm6.icons.lod", {"CNDLABRA": bitmap("cndlabra", (220, 130, 10)),
                                     "herb03": bitmap("herb03", (100, 30, 180))}, version="MMVI")
    lod(root / "Data/icons.lod", {name: bitmap(name, (255, 210, 30))
                                 for name in ["item204", "item205", "item206"]}, version="MMVI")
    lod(root / "Data/mm6.sprites.lod", {"cndlabra": b"unrelated sprite namespace"})


def test_all_referenced_items_and_gold_import_without_gameplay_changes(tmp_path):
    root, assets, out = [tmp_path / n for n in ["source", "assets", "out"]]
    fixture(root, assets)
    before = {p: p.read_bytes() for d in [root, assets] for p in d.rglob("*") if p.is_file()}
    result = run(root, assets, out)
    assert result.returncode == 0, result.stderr
    report = json.loads((out / "item_icon_conversion.json").read_text())
    assert report["imported_images"] == 5 and report["requested_images"] == 6
    assert report["missing_images"] == ["absent"]
    refs = {r["name"]: r for r in report["references"]}
    assert refs["cndlabra"]["item_ids"] == [700]
    assert refs["herb03"]["item_ids"] == [773]
    assert refs["item204"]["item_ids"] == []
    for name, color in [("cndlabra", (220, 130, 10)), ("herb03", (100, 30, 180)),
                        ("item204", (255, 210, 30)), ("item205", (255, 210, 30)),
                        ("item206", (255, 210, 30))]:
        path = out / refs[name]["path"]
        image = Image.open(path).convert("RGB")
        assert list(image.getdata()) == [(0, 0, 0), color, color, (0, 0, 0), color, color]
        assert refs[name]["width"] == 3 and refs[name]["height"] == 2
        assert refs[name]["archive"] == ("Data/icons.lod" if name.startswith("item") else "Data/mm6.icons.lod")
        assert path.stat().st_mode & 0o777 == 0o644
    assert all(p.read_bytes() == data for p, data in before.items())
    assert not report["runtime_ready"] and not report["visual_gameplay_validated"]


@pytest.mark.parametrize("version,compressed,mips", [
    ("MMVI", True, False), ("MMVI", False, False),
    ("MMVIII", True, False), ("MMVIII", True, True),
])
def test_item_images_preserve_palette_indices_including_duplicate_colors(tmp_path, version, compressed, mips):
    root, assets, out = [tmp_path / n for n in ["source", "assets", "out"]]
    fixture(root, assets)
    width, height = (8, 8) if mips else (3, 2)
    pixels = bytes(([0, 1, 255, 254, 2, 0] * 11)[:width * height])
    palette = bytearray((i * 37) % 256 for i in range(768))
    # hammer1 uses this unusual palette-zero color. A matching nonzero entry must retain its identity.
    palette[:6] = bytes([252, 0, 216, 252, 0, 216])
    payload = pixels + (bytes(range(21)) if mips else b"")
    lod(root / "Data/mm6.icons.lod", {"cndlabra": wrapped(
        "cndlabra", payload, width, height, palette, compressed,
        name_size=64 if version == "MMVIII" else 16)}, version=version)
    result = run(root, assets, out)
    assert result.returncode == 0, result.stderr
    path = out / "engine/icons/cndlabra.bmp"
    raw = path.read_bytes()
    assert struct.unpack_from("<H", raw, 28)[0] == 8
    with Image.open(path) as image:
        assert image.mode == "P" and image.size == (width, height)
        assert image.tobytes() == pixels
        assert bytes(image.getpalette()) == palette
        assert image.convert("RGB").tobytes() == b"".join(palette[i * 3:i * 3 + 3] for i in pixels)


@pytest.mark.parametrize("fault", ["cross_world", "unknown_layer", "broken_image", "escape_archive", "unsafe_name"])
def test_bad_item_images_leave_no_partial_inventory_output(tmp_path, fault):
    root, assets, out = [tmp_path / n for n in ["source", "assets", "out"]]
    if fault == "unsafe_name":
        def mutate(base, locale, settings, items, notes):
            items[0][1] = "../escape"
            base["items.txt"][0][1] = "../escape"
        source(root, assets, mutate=mutate)
    else:
        fixture(root, assets)
        if fault in ["cross_world", "unknown_layer"]:
            name = "mm7.icons.lod" if fault == "cross_world" else "custom.icons.lod"
            lod(root / "Data" / name, {"cndlabra": bitmap("cndlabra", (20, 30, 40))}, version="MMVI")
        elif fault == "broken_image":
            lod(root / "Data/mm6.icons.lod", {"cndlabra": b"bad image"})
        else:
            target = tmp_path / "outside.lod"
            (root / "Data/mm6.icons.lod").rename(target)
            (root / "Data/mm6.icons.lod").symlink_to(target)
    result = run(root, assets, out)
    assert result.returncode != 0
    assert not out.exists()
    if fault in ["cross_world", "unknown_layer"]:
        assert "Ambiguous item icon ownership" in result.stderr
    elif fault == "unsafe_name":
        assert "Invalid item icon member name" in result.stderr
    elif fault == "escape_archive":
        assert "Item icon archive escapes source" in result.stderr
