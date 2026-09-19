"""Exercise native layout packaging and original namespace/skin precedence with synthetic LODs."""
from pathlib import Path
import json
import os
import subprocess

from PIL import Image
import pytest

from test_menu_resources import lod, wrapped

REPO = Path(__file__).parents[1]
TOOL = Path(os.environ.get("UI_IMPORT_TOOL", REPO / "build-apple/ui_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build ui_import first")


def picture(name, rgb):
    palette = bytearray(768)
    palette[3:6] = bytes(rgb)
    return wrapped(name, bytes([1] * 6), width=3, height=2, palette=palette)


DIMENSION_DOOR_IMAGES = (["tpglobal"] + [f"tpjadam{i}" for i in range(1, 9)]
                         + [f"tpantag{i}" for i in range(1, 9)]
                         + [f"tpenroth{i}" for i in range(1, 6)])


def add_dimension_door_layout_and_table(prepared):
    root, layouts, _ = prepared
    (layouts / "town_portal.yml").write_text(
        "screen: TownPortal\nelements:\n"
        "  - id: Background\n    anchor: top_left\n    asset:\n      default: Basebar\n")
    rows = [
        ["#", "Map", "X", "Y", "Z", "Direction", "LookAngle", "IconName", "Icon X", "Icon Y",
         "IconWidth", "IconHeight", "QBitIndex", "Description"],
        ["@", "TPGlobal", "309"],
        ["0", "0", "0", "0", "0", "0", "0", "TPJadam1", "0", "46", "", "", "309", "Jadame"],
        ["1", "0", "0", "0", "0", "0", "0", "none", "0", "0", "", "", "309", ""],
        ["2", "0", "0", "0", "0", "0", "0", "TPAntag1", "180", "46", "", "", "309",
         "Antagarich"],
        ["3", "0", "0", "0", "0", "0", "0", "none", "0", "0", "", "", "309", ""],
        ["4", "0", "0", "0", "0", "0", "0", "TPEnroth1", "180", "0", "", "", "309", "Enroth"],
        ["5", "0", "0", "0", "0", "0", "0", "none", "0", "0", "", "", "309", ""],
    ]
    (root / "Data/Tables/TownPortalSwitch.txt").write_text(
        "\n".join("\t".join(row) for row in rows) + "\n")


@pytest.fixture
def prepared(tmp_path):
    root, layouts, output = (tmp_path / name for name in ("source", "layouts", "output"))
    data = root / "Data"
    (data / "Tables").mkdir(parents=True)
    (data / "Additional UI").mkdir()
    layouts.mkdir()
    (data / "Tables/Additional UI.txt").write_text(
        "#\tLod name\tD Lod name\tShow\tY\tX\tOn top\tY\tX\tNotes\n"
        "1\tdefault\t\t-\t458\t0\t-\t389\t0\tDefault\n"
        "2\t7UI.icons.lod\t\tx\t378\t-3\tx\t384\t-4\tSeven\n"
        "3\t6UI.icons.lod\t6UI.EnglishD.lod\t-\t458\t0\t-\t389\t0\tSix\n")
    (layouts / "gameplay.yml").write_text(
        "screen: OutdoorHud\nelements:\n"
        "  - id: Panel\n    anchor: top_left\n    width: 3\n    height: 2\n    asset:\n      default: Basebar\n"
        "  - id: Label\n    anchor: top_left\n    text:\n      font: Lucida\n"
        "  - id: Button\n    anchor: top_left\n    asset:\n      default: Button\n"
        "  - id: Missing\n    anchor: top_left\n    asset:\n      default: MissingArt\n")
    lod(data / "icons.lod", {"basebar": picture("basebar", (1, 2, 3))})
    lod(data / "mmmerge.icons.lod", {"basebar": picture("basebar", (4, 5, 6))})
    lod(data / "mm8.D.lod", {"button": picture("button", (7, 8, 9))})
    lod(data / "10 LocZHCN.EnglishD.lod", {"button": picture("button", (10, 11, 12))})
    lod(data / "Additional UI/7UI.icons.lod", {"basebar": picture("basebar", (13, 14, 15))})
    lod(data / "Additional UI/6UI.icons.lod", {"basebar": picture("basebar", (16, 17, 18))})
    lod(data / "Additional UI/6UI.EnglishD.lod", {"button": picture("button", (19, 20, 21))})
    lod(data / "mmmerge.T.lod", {"lucida.fnt": wrapped("lucida.fnt", b"original-font")})
    lod(data / "10 LocZHCN.EnglishT.lod", {"lucida.fnt": wrapped("lucida.fnt", b"localized-font")})
    return root, layouts, output


def run(prepared, ui=3):
    root, layouts, output = prepared
    return subprocess.run([str(TOOL), str(root), str(layouts), str(ui), str(output)],
                          capture_output=True, text=True, timeout=20, umask=0o077)


@pytest.mark.parametrize("ui,base,button", [(1, (4, 5, 6), (10, 11, 12)),
                                         (2, (13, 14, 15), (10, 11, 12)),
                                         (3, (16, 17, 18), (19, 20, 21))])
def test_original_skin_and_localization_layers(prepared, ui, base, button):
    root, layouts, output = prepared
    before = {p: p.read_bytes() for folder in (root, layouts) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared, ui)
    assert result.returncode == 0, result.stderr
    assert Image.open(output / "engine/icons/basebar.bmp").convert("RGB").getpixel((0, 0)) == base
    assert Image.open(output / "engine/icons/button.bmp").convert("RGB").getpixel((0, 0)) == button
    assert (output / "engine/icons/lucida.fnt").read_bytes() == b"localized-font"
    assert (output / "engine/ui/gameplay/gameplay.yml").read_bytes() == (layouts / "gameplay.yml").read_bytes()
    report = json.loads((output / "ui_conversion.json").read_text())
    assert report["ui_set_id"] == ui and report["layouts"] == 1 and report["layout_elements"] == 4
    assert report["image_references"] == 3 and report["font_references"] == 1
    assert report["dimension_door_image_references"] == 0
    assert len(report["images"]) == 2 and len(report["fonts"]) == 1
    assert report["missing_images"] == ["missingart"] and report["missing_fonts"] == []
    assert report["missing_dimension_door_images"] == []
    assert report["runtime_images_decoded"] and not report["complete_hud_validated"]
    assert not report["font_rendering_validated"] and not report["runtime_ready"]
    assert all(p.read_bytes() == value for p, value in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["missing_skin", "damaged_image", "namespace_conflict",
                                    "bad_layout", "unsafe_reference", "missing_layout", "unknown_set"])
def test_invalid_inputs_do_not_publish(prepared, fault):
    root, layouts, output = prepared
    data = root / "Data"
    if fault == "missing_skin":
        (data / "Additional UI/6UI.icons.lod").unlink()
    elif fault == "damaged_image":
        lod(data / "Additional UI/6UI.icons.lod", {"basebar": b"broken"})
    elif fault == "namespace_conflict":
        lod(data / "Additional UI/6UI.EnglishD.lod", {"basebar": picture("basebar", (1, 2, 3))})
    elif fault == "missing_layout":
        (layouts / "gameplay.yml").unlink()
    elif fault in ("bad_layout", "unsafe_reference"):
        (layouts / "gameplay.yml").write_text("broken: [" if fault == "bad_layout" else
            "screen: OutdoorHud\nelements:\n  - id: X\n    anchor: top_left\n    asset:\n      default: ../escape\n")
    result = run(prepared, 99 if fault == "unknown_set" else 3)
    assert result.returncode != 0 and not output.exists(), result.stdout


def test_absent_font_is_reported_without_placeholder(prepared):
    root, layouts, output = prepared
    (root / "Data/mmmerge.T.lod").unlink()
    (root / "Data/10 LocZHCN.EnglishT.lod").unlink()
    assert run(prepared).returncode == 0
    report = json.loads((output / "ui_conversion.json").read_text())
    assert report["missing_fonts"] == ["lucida.fnt"]
    assert not (output / "engine/icons/lucida.fnt").exists()


def test_town_portal_layout_imports_dynamic_dimension_door_images_with_archive_precedence(prepared):
    root, _, output = prepared
    add_dimension_door_layout_and_table(prepared)
    base_entries = {"basebar": picture("basebar", (1, 2, 3))}
    base_entries.update({name: picture(name, (20, 30, 40)) for name in DIMENSION_DOOR_IMAGES})
    lod(root / "Data/icons.lod", base_entries)
    lod(root / "Data/mmmerge.icons.lod", {
        "basebar": picture("basebar", (4, 5, 6)),
        "TPGLOBAL": picture("TPGLOBAL", (90, 80, 70)),
        "TPJADAM8": picture("TPJADAM8", (60, 50, 40)),
    })

    result = run(prepared, 1)
    assert result.returncode == 0, result.stderr
    assert all((output / f"engine/icons/{name}.bmp").is_file() for name in DIMENSION_DOOR_IMAGES)
    assert Image.open(output / "engine/icons/tpglobal.bmp").convert("RGB").getpixel((0, 0)) == (90, 80, 70)
    assert Image.open(output / "engine/icons/tpjadam8.bmp").convert("RGB").getpixel((0, 0)) == (60, 50, 40)
    assert Image.open(output / "engine/icons/tpantag8.bmp").convert("RGB").getpixel((0, 0)) == (20, 30, 40)
    report = json.loads((output / "ui_conversion.json").read_text())
    assert report["dimension_door_image_references"] == 22
    assert report["missing_dimension_door_images"] == []
    assets = {asset["name"]: asset for asset in report["images"]}
    assert assets["tpglobal.bmp"]["archive"] == "Data/mmmerge.icons.lod"
    assert assets["tpjadam8.bmp"]["archive"] == "Data/mmmerge.icons.lod"


def test_missing_dynamic_dimension_door_images_are_reported_without_placeholders(prepared):
    root, _, output = prepared
    add_dimension_door_layout_and_table(prepared)
    lod(root / "Data/mmmerge.icons.lod", {
        "basebar": picture("basebar", (4, 5, 6)),
        "tpglobal": picture("tpglobal", (9, 8, 7)),
    })

    result = run(prepared, 1)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / "ui_conversion.json").read_text())
    expected_missing = sorted(set(DIMENSION_DOOR_IMAGES) - {"tpglobal"})
    assert report["missing_dimension_door_images"] == expected_missing
    assert set(expected_missing).issubset(report["missing_images"])
    assert (output / "engine/icons/tpglobal.bmp").is_file()
    assert all(not (output / f"engine/icons/{name}.bmp").exists() for name in expected_missing)


@pytest.mark.parametrize("original,replacement", [
    ("TPJadam1", "TPJadam2"),
    ("Jadame", "Changed"),
])
def test_modified_tpglobal_profile_does_not_publish(prepared, original, replacement):
    root, _, output = prepared
    add_dimension_door_layout_and_table(prepared)
    table = root / "Data/Tables/TownPortalSwitch.txt"
    table.write_text(table.read_text().replace(original, replacement))
    result = run(prepared, 1)
    assert result.returncode != 0 and not output.exists(), result.stdout


@pytest.mark.parametrize("kind", ["layout", "archive", "table"])
def test_source_escape_rejected(prepared, kind, tmp_path):
    root, layouts, output = prepared
    path = {"layout": layouts / "gameplay.yml", "archive": root / "Data/Additional UI/6UI.icons.lod",
            "table": root / "Data/Tables/Additional UI.txt"}[kind]
    outside = tmp_path / "outside"
    path.rename(outside)
    path.symlink_to(outside)
    assert run(prepared).returncode != 0 and not output.exists()


def test_existing_and_nested_output_preserved(prepared):
    root, layouts, output = prepared
    for parent in (root, layouts):
        assert run((root, layouts, parent / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"


def region_layout(prepared, declaration):
    root, layouts, output = prepared
    (layouts / "gameplay.yml").write_text(
        "screen: OutdoorHud\ntexture_regions:\n" + declaration + "\nelements:\n"
        "  - id: Panel\n    anchor: top_left\n    asset: Cropped\n")


REGION = "  - {name: Cropped, source: Basebar, source_width: 3, source_height: 2, x: 1, y: 0, width: 2, height: 2}"


def test_region_imports_original_source_without_a_derived_bitmap(prepared):
    root, layouts, output = prepared
    region_layout(prepared, REGION)
    before = (layouts / "gameplay.yml").read_bytes()
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / "ui_conversion.json").read_text())
    assert [i["name"] for i in report["images"]] == ["basebar.bmp"]
    assert report["texture_regions"] == [{"name": "cropped", "source": "basebar", "x": 1, "y": 0,
        "width": 2, "height": 2, "source_width": 3, "source_height": 2, "available": True}]
    assert report["missing_images"] == []
    assert not (output / "engine/icons/cropped.bmp").exists()
    assert (output / "engine/ui/gameplay/gameplay.yml").read_bytes() == before


@pytest.mark.parametrize("fault", ["negative", "outside", "empty", "shape", "unsafe", "duplicate", "chain"])
def test_invalid_texture_region_does_not_publish(prepared, fault):
    declaration = REGION
    if fault == "negative":
        declaration = declaration.replace("x: 1", "x: -1")
    elif fault == "outside":
        declaration = declaration.replace("width: 2", "width: 3")
    elif fault == "empty":
        declaration = declaration.replace("width: 2", "width: 0")
    elif fault == "shape":
        declaration = declaration.replace("source_width: 3", "source_width: 4")
    elif fault == "unsafe":
        declaration = declaration.replace("source: Basebar", "source: ../Basebar")
    elif fault == "duplicate":
        declaration += "\n" + REGION
    elif fault == "chain":
        declaration += "\n" + REGION.replace("name: Cropped", "name: Basebar").replace("source: Basebar", "source: Other")
    region_layout(prepared, declaration)
    result = run(prepared)
    assert result.returncode != 0 and not prepared[2].exists(), result.stdout
