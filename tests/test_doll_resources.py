"""Original zero-based portraits, merged layouts and shared Party identity behavior."""
import csv
import json
import os
from pathlib import Path
import subprocess

import pytest
from PIL import Image

from test_roster_resources import records, write
from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("DOLL_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/doll_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build doll_import first")
PORTRAIT_HEADER = ("#,Paperdoll type,Def class,Def voice,Def sex,Available at start,Body X,Body Y,Helm X,Helm Y,"
                   "Background,bod,head,LHd,LHu,LHo,RHb,Unk,Unk,RHd,RHu,Face pics prefix,DefAttack (melee),"
                   "DefAttack (range),NPCPic,Race,Notes").split(",")
DOLL_HEADER = ("#,Bow,Armor,Helm,Belt,Boots,Cloak,Weapon,RHo X,RHo Y,RHc X,RHc Y,RHf X,RHf Y,LHo X,LHo Y,"
               "LHc X,LHc Y,LHf X,LHf Y,OH Offset X,OH Offset Y,MH Offset X,MH Offset Y,Bow Offset X,"
               "Bow Offset Y,Shield X,Shield Y").split(",")


def tables():
    portraits = [PORTRAIT_HEADER.copy()]
    for id_ in range(3):
        row = [str(id_), "0", "16", str(10 + id_), str(id_ % 2), "x", "30", "33", "2", "1",
               "Back", "Body", "null", "LHd", "LHu", "LHo", "RHb", "Unused", "LHu", "RHd", "RHu",
               "pc01-", "0", "137", "7021", str(0 if id_ == 0 else 9), 'Source "note"\nSecond line']
        if id_ == 2:
            row[1:3] = ["1", "-1"]
            row[5] = "-"
            row[21] = "WetS_"
            row[23] = "0"
            row[25] = "-1"
        portraits.append(row)
    dolls = [DOLL_HEADER.copy(), ["0"] + ["x"] * 7 + [str(i + 1) for i in range(20)],
             ["1"] + ["-"] * 7 + [str(-i - 1) for i in range(20)]]
    return portraits, dolls


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    portraits, dolls = tables()
    write(root / "Data/Tables/Character portraits.txt", portraits)
    write(root / "Data/Tables/Character doll types.txt", dolls)
    write(assets / "engine/data_tables/class_extra.txt", [["#", "Kind", "Step", "Note"],
                                                         ["16", "6", "0", "Knight"]])
    roster = records()[:2]
    for id_ in range(2):
        row = records()[3].copy()
        row[0] = str(id_ + 1)
        row[2] = "16"
        row[4] = str(id_)
        roster.append(row)
    write(assets / "engine/data_tables/roster.txt", roster)
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def read(path):
    with path.open() as stream:
        return list(csv.reader(stream, delimiter="\t"))


def test_zero_identity_shared_portrait_helm_hands_and_source_cells(prepared):
    root, assets, output = prepared
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    portraits, dolls = tables()
    assert read(output / "engine/data_tables/character_data.txt") == portraits
    assert read(output / "engine/data_tables/doll_types.txt") == dolls
    report = json.loads((output / "doll_conversion.json").read_text())
    assert report["portraits"] == 3 and report["doll_types"] == 2
    assert report["identity_checks"] == report["selectable_portraits"] == 2
    assert report["special_attack_portraits"] == 2
    assert report["unavailable_class_ids"] == [2] and report["engine_character_id_offset"] == 1
    assert report["typed_validation"] is True and report["source_id_preserved"] is True
    assert not report["runtime_ready"] and not report["rendered_dolls_validated"]
    assert report["images_exported"] == 0 and not report["special_attacks_connected"]
    assert all(p.read_bytes() == content for p, content in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["header", "width", "id_gap", "class", "race", "sex", "doll_type", "flag",
                                    "asset_path", "helm_overflow", "voice", "attack", "unknown_selectable"])
def test_invalid_portrait_never_publishes_output(prepared, fault):
    portraits, _ = tables()
    if fault == "header":
        portraits[0][8] = "Background"
    elif fault == "width":
        portraits[1].pop()
    elif fault == "unknown_selectable":
        portraits[3][5] = "x"
    else:
        column, value = {"id_gap": (0, "1"), "class": (2, "99"), "race": (25, "11"), "sex": (4, "2"),
                         "doll_type": (1, "9"), "flag": (5, "yes"), "asset_path": (11, "../body"),
                         "helm_overflow": (8, "32768"), "voice": (3, "256"), "attack": (23, "65536")}[fault]
        portraits[1][column] = value
    write(prepared[0] / "Data/Tables/Character portraits.txt", portraits)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


@pytest.mark.parametrize("fault", ["order", "flag", "coordinate"])
def test_invalid_doll_type_never_publishes_output(prepared, fault):
    _, dolls = tables()
    if fault == "order":
        dolls[2][0] = "0"
    elif fault == "flag":
        dolls[1][1] = "yes"
    else:
        dolls[1][16] = "-32769"
    write(prepared[0] / "Data/Tables/Character doll types.txt", dolls)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_crlf_and_signed_coordinate_boundaries(prepared):
    root, _, _ = prepared
    portraits, dolls = tables()
    portraits[1][6:10] = ["-32768", "32767", "32767", "-32768"]
    dolls[1][14:18] = ["-32768", "32767", "32767", "-32768"]
    write(root / "Data/Tables/Character portraits.txt", portraits)
    write(root / "Data/Tables/Character doll types.txt", dolls)
    for path in (root / "Data/Tables").iterdir():
        path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
    result = run(prepared)
    assert result.returncode == 0, result.stderr


def test_missing_roster_portrait_and_path_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    rows = read(assets / "engine/data_tables/roster.txt")
    rows[2][4] = "200"
    write(assets / "engine/data_tables/roster.txt", rows)
    assert run(prepared).returncode != 0 and not output.exists()
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    path = root / "Data/Tables/Character portraits.txt"
    outside = tmp_path / "outside.txt"
    path.rename(outside)
    path.symlink_to(outside)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()


def doll_bitmap(name, color):
    palette = bytearray(768)
    palette[3:6] = bytes(color)
    return wrapped(name, bytes([0, 1, 1, 0, 1, 1]), 3, 2, palette, name_size=16)


def test_referenced_body_hands_background_export_exact_pixels_and_missing_records(prepared):
    root, assets, output = prepared
    colors = {'body': (210, 40, 80), 'back': (50, 100, 150), 'rhd': (100, 220, 140)}
    lod(root / 'Data/mm8.icons.lod',
        {name.upper(): doll_bitmap(name, color) for name, color in colors.items()}, version='MMVI')
    # Neither an ignored source column nor a face prefix belongs to the body-image selection.
    lod(root / 'Data/custom.icons.lod', {'unused': b'not an image', 'pc01-01': b'not an image'})
    lod(root / 'Data/mm8.sprites.lod', {'body': b'wrong image namespace'})
    before = {p: p.read_bytes() for directory in [root, assets] for p in directory.rglob('*') if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / 'doll_conversion.json').read_text())
    assert report['images_exported'] == 3
    assert report['missing_images'] == ['lhd', 'lho', 'lhu', 'rhb', 'rhu']
    assert report['paperdoll_image_references'] == 8
    images = {image['name']: image for image in report['images']}
    for name, color in colors.items():
        record = images[name + '.bmp']
        assert record['character_ids'] == [1, 2, 3]
        assert record['archive'] == 'mm8.icons.lod'
        path = output / 'engine/icons' / record['name']
        image = Image.open(path).convert('RGB')
        assert image.size == (3, 2) == (record['width'], record['height'])
        assert list(image.getdata()) == [(0, 0, 0), color, color, (0, 0, 0), color, color]
        assert path.stat().st_mode & 0o777 == 0o644
    assert all(p.read_bytes() == content for p, content in before.items())
    assert not report['rendered_dolls_validated'] and not report['runtime_ready']


@pytest.mark.parametrize('original_first', [False, True])
def test_known_merge_paperdoll_overrides_original_icons_independent_of_scan_order(prepared, original_first):
    root, _, output = prepared
    layers = [('ICONS.LOD', (220, 0, 0)), ('mmmerge.icons.lod', (0, 220, 0))]
    for name, color in layers if original_first else reversed(layers):
        lod(root / 'Data' / name, {'body': doll_bitmap('body', color)}, version='MMVI')
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    image = Image.open(output / 'engine/icons/body.bmp').convert('RGB')
    assert image.getpixel((1, 0)) == (0, 220, 0)
    report = json.loads((output / 'doll_conversion.json').read_text())
    assert report['images'][0]['archive'] == 'mmmerge.icons.lod'


@pytest.mark.parametrize('fault', ['unknown_overlay', 'cross_world', 'corrupt_image', 'escaping_archive'])
def test_paperdoll_source_failures_roll_back_data_and_images(prepared, tmp_path, fault):
    root, _, output = prepared
    image = doll_bitmap('body', (20, 30, 40))
    path = root / 'Data/mm8.icons.lod'
    lod(path, {'body': image}, version='MMVI')
    if fault in ['unknown_overlay', 'cross_world']:
        name = 'custom.icons.lod' if fault == 'unknown_overlay' else 'mm7.icons.lod'
        lod(root / 'Data' / name, {'body': image}, version='MMVI')
    elif fault == 'corrupt_image':
        lod(path, {'back': doll_bitmap('back', (1, 2, 3)), 'body': b'invalid image'}, version='MMVI')
    else:
        outside = tmp_path / 'outside.lod'
        path.rename(outside)
        path.symlink_to(outside)
    result = run(prepared)
    assert result.returncode != 0
    assert not output.exists()
