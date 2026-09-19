"""Equipment categories, body/collar coordinates and portrait-specific offsets."""
import json
import os
from pathlib import Path
import subprocess

import pytest
from PIL import Image

from test_bolster_resources import source_rows, write_source
from test_doll_resources import read, tables
from test_roster_resources import write
from test_doll_resources import doll_bitmap
from test_menu_resources import lod


def picture_items(prepared):
    path = prepared[1] / 'engine/data_tables/items.txt'
    rows = read(path)
    for row in rows:
        row[1] = 'gear' + row[0]
    write(path, rows)


def test_original_fitted_images_body_rules_collar_layers_and_pixels(prepared):
    root, assets, output = prepared
    picture_items(prepared)
    path = assets / 'engine/data_tables/doll_types.txt'
    dolls = read(path)
    dolls[3][3] = '-'  # Minotaurs cannot wear helms or boots.
    dolls[3][5] = '-'
    dolls[6][1:8] = ['-'] * 7  # Original dragon body cannot wear this equipment.
    write(path, dolls)
    names = ['gear10v1', 'gear10v1a', 'gear10v4', 'gear10v4a', 'gear11v2',
             'gear12v3', 'gear13v5', 'gear14v3a', 'gear14v3b', 'gear14v3c']
    colors = {n: (20 + i, 90 + i, 150 + i) for i, n in enumerate(names)}
    images = {n: doll_bitmap(n, colors[n]) for n in names}
    # These are not references: forbidden slots, inventory icons and unrelated LOD members.
    images.update({n: doll_bitmap(n, (200, 0, 0)) for n in ['gear11v3', 'gear13v3', 'gear10v6', 'gear10']})
    lod(root / 'Data/mm8.icons.lod', images, version='MMVI')
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob('*') if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / 'equipment_picture_conversion.json').read_text())
    assert report['images_exported'] == len(names)
    assert report['image_references'] == 38
    assert len(report['missing_images']) == 28
    assert {p.stem for p in (output / 'engine/icons').glob('*.bmp')} == set(names)
    for entry in report['images']:
        name = Path(entry['name']).stem
        assert entry['archive'] == 'mm8.icons.lod'
        assert entry['item_ids'] == [int(name[4:6])]
        path = output / 'engine/icons' / entry['name']
        image = Image.open(path).convert('RGB')
        assert image.size == (3, 2)
        assert image.tobytes() == bytes((0, 0, 0) + colors[name] * 2 + (0, 0, 0) + colors[name] * 2)
        assert path.stat().st_mode & 0o777 == 0o644
    assert all(p.read_bytes() == data for p, data in before.items())


@pytest.mark.parametrize('base,patch', [('icons.lod', 'mm8.icons.lod'), ('mm6.icons.lod', 'mmmerge.icons.lod')])
@pytest.mark.parametrize('reverse', [False, True])
def test_original_equipment_patch_ownership_is_independent_of_enumeration(prepared, base, patch, reverse):
    root, _, output = prepared
    picture_items(prepared)
    archives = [(base, (200, 0, 0)), (patch, (0, 220, 0))]
    if reverse:
        archives.reverse()
    for name, color in archives:
        lod(root / 'Data' / name, {'gear10v1': doll_bitmap('gear10v1', color)}, version='MMVI')
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    path = output / 'engine/icons/gear10v1.bmp'
    assert Image.open(path).convert('RGB').getpixel((1, 0)) == (0, 220, 0)
    report = json.loads((output / 'equipment_picture_conversion.json').read_text())
    assert report['images'][0]['archive'] == patch


@pytest.mark.parametrize('fault', ['unknown_overlay', 'cross_world', 'corrupt_image', 'escaping_archive'])
def test_equipment_image_failure_rolls_back_entire_stage(prepared, tmp_path, fault):
    root, _, output = prepared
    picture_items(prepared)
    path = root / 'Data/mm8.icons.lod'
    images = {'gear10v1': doll_bitmap('gear10v1', (20, 30, 40))}
    lod(path, images, version='MMVI')
    if fault in ['unknown_overlay', 'cross_world']:
        name = 'custom.icons.lod' if fault == 'unknown_overlay' else 'mm7.icons.lod'
        lod(root / 'Data' / name, images, version='MMVI')
    elif fault == 'corrupt_image':
        images['gear11v1'] = b'corrupt'
        lod(path, images, version='MMVI')
    else:
        outside = tmp_path / 'outside.lod'
        path.rename(outside)
        path.symlink_to(outside)
    result = run(prepared)
    assert result.returncode != 0
    assert not output.exists()

TOOL = Path(os.environ.get("EQUIPMENT_PICTURE_IMPORT_TOOL",
                          Path(__file__).parents[1] / "build-apple/equipment_picture_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build equipment_picture_import first")
NAMES = {"Complex item pictures.txt": "complex_item_pictures.txt",
         "Complex item pictures offsets.txt": "complex_item_picture_offsets.txt"}


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    top = ["", "", "Armors", "Settings"] + sum(([f"t{i % 6}", ""] for i in range(12)), [])
    rows = [top[:-1], ["Id", "Item id", "Notes", ""] + ["X", "Y"] * 12]
    for index, section in enumerate(["Armors", "Helms", "Belts", "Boots", "Cloaks"]):
        if index:
            header = ["", "", section, "n/u" if section == "Boots" else "Complex"]
            if section == "Cloaks":
                header += [""] * 12 + ["Collars"] + [""] * 11
            rows.extend([[""], header])
        body = [str(i + 1 if i % 2 == 0 else -i) for i in range(12)]
        body[-2:] = ["", ""]
        row = ["1", str(10 + index), f'Original "{section}"', "x" if index in (1, 2, 4) else ""] + body
        if section == "Cloaks":
            row += [str(100 + i) for i in range(10)]  # Absent last collar pair defaults to zero.
        rows.append(row)
    write_source(root / "Data/Tables/Complex item pictures.txt", rows)
    write_source(root / "Data/Tables/Complex item pictures offsets.txt",
                 [["Portrait id", "Item id", "X", "Y"], ["0", "10", "4", "-3"], ["2", "14", "-2", "7"]])
    base = assets / "engine/data_tables"
    items = []
    for id_, kind in [(10, "Armor"), (11, "Helm"), (12, "Belt"), (13, "Boots"), (14, "Cloak"), (99, "Armor")]:
        if id_ in (10, 14):
            kind = kind.lower()  # Four real source items use lowercase types; runtime treats them identically.
        items.append([str(id_), "image", f"物品{id_}", "1", kind, "Misc", "0", "0", "0", "0",
                      "未鉴定", "0", "0", "0", "0", "0", "中文物品说明"])
    write(base / "items.txt", items)
    portraits, dolls = tables()
    write(base / "character_data.txt", portraits)
    dolls = dolls[:1] + [[str(i)] + dolls[1][1:] for i in range(6)]
    write(base / "doll_types.txt", dolls)
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def test_sections_local_ids_layers_and_offset_isolation(prepared):
    root, assets, output = prepared
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    for source, target in NAMES.items():
        assert read(output / "engine/data_tables" / target) == source_rows(root / "Data/Tables" / source)
    report = json.loads((output / "equipment_picture_conversion.json").read_text())
    assert report["items"] == 5 and report["items_by_section"] == [1] * 5
    assert report["complex_flags_by_section"] == [0, 1, 1, 0, 1]
    assert report["portrait_item_offsets"] == 2 and report["point_checks"] == 90
    assert report["equipment_ids_without_source_coordinates"] == [99]
    assert report["typed_validation"] and report["images_exported"] == 0
    assert not report["rendered_equipment_validated"] and not report["collar_layers_rendered"]
    assert not report["runtime_ready"]
    assert all(p.read_bytes() == data for p, data in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["header", "section_order", "local_id", "duplicate_item", "category",
                                    "coordinate", "width", "complex_flag", "collar_coordinate"])
def test_invalid_picture_is_not_published(prepared, fault):
    path = prepared[0] / "Data/Tables/Complex item pictures.txt"
    rows = source_rows(path)
    if fault == "width":
        rows[2].pop()
    else:
        row, column, value = {
            "header": (0, 4, "t1"), "section_order": (4, 2, "Belts"), "local_id": (5, 0, "2"),
            "duplicate_item": (5, 1, "10"), "category": (2, 1, "13"), "coordinate": (2, 4, "32768"),
            "complex_flag": (5, 3, "yes"), "collar_coordinate": (14, 16, "not-a-number"),
        }[fault]
        rows[row][column] = value
    write_source(path, rows)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


@pytest.mark.parametrize("fault", ["portrait", "item", "duplicate", "offset_overflow"])
def test_invalid_offset_is_not_published(prepared, fault):
    path = prepared[0] / "Data/Tables/Complex item pictures offsets.txt"
    rows = source_rows(path)
    if fault == "duplicate":
        rows.append(rows[1])
    else:
        column, value = {"portrait": (0, "3"), "item": (1, "99"), "offset_overflow": (2, "-32769")}[fault]
        rows[1][column] = value
    write_source(path, rows)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_signed_coordinate_sum_exceeds_sixteen_bits_without_wrapping(prepared):
    source = prepared[0] / "Data/Tables"
    path = source / "Complex item pictures.txt"
    rows = source_rows(path)
    rows[2][4:6] = ["32767", "-32768"]
    write_source(path, rows)
    path = source / "Complex item pictures offsets.txt"
    offsets = source_rows(path)
    offsets[1][2:] = ["32767", "-32768"]
    write_source(path, offsets)
    result = run(prepared)
    assert result.returncode == 0, result.stderr


def test_output_and_symlink_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    path = root / "Data/Tables/Complex item pictures.txt"
    outside = tmp_path / "outside.txt"
    path.rename(outside)
    path.symlink_to(outside)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()
