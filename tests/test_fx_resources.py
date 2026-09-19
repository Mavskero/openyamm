"""Fixed MM8 binary-profile integration; the private EXE fixture is never executed.

Set MM8_EXE_FIXTURE to an owned copy. No proprietary binary is bundled in tests.
Prerequisite tables and source labels are synthetic; actual source equivalence is
also checked independently in the local validation artifacts.
"""
import csv
import io
import json
import os
from pathlib import Path
import subprocess

import pytest

TOOL = Path(os.environ.get("FX_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/fx_import"))
EXE = Path(os.environ.get("MM8_EXE_FIXTURE", "/Users/yukarii/Downloads/MM678_Merged_05.11.2023_gdrv/mm8.exe"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build fx_import first")


def write(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        csv.writer(stream, delimiter="\t", lineterminator="\n").writerows(rows)


def read(path):
    return list(csv.reader(io.StringIO(path.read_text()), delimiter="\t"))


@pytest.fixture
def prepared(tmp_path):
    if not EXE.is_file():
        pytest.skip("Supply an owned mm8.exe via MM8_EXE_FIXTURE")
    root, base, out = tmp_path / "source", tmp_path / "base", tmp_path / "out"
    root.mkdir()
    (root / "mm8.exe").write_bytes(EXE.read_bytes())
    (root / "Scripts/Core").mkdir(parents=True)
    (root / "Scripts/Core/ConstAndBits.lua").write_bytes((
        "const.FaceAnimation = {\n KillSmallEnemy = 1,\n CastSpell = 49, -- spell\n"
        " LookDown = 64,\n Awaken = 103,\n}\n").replace("\n", "\r\n").encode())
    tables = base / "engine/data_tables"
    write(tables / "spells.txt", [[i, i, f"魔法{i}", "none", f"魔法{i}", "说明"] for i in range(1, 133)])
    names = [f"spell{i:02}" for i in (3, 5, 14, 17, 25, 36, 38, 46, 51, 55, 58, 69, 71, 73, 75)]
    names += [f"sp{kind}{i}" for kind in ("heal", "boost") for i in range(1, 4)]
    names += ["zapp"]
    write(tables / "icon_frame_data.txt", [[name, name + "a", 1, 1, 4] for name in names])
    write(tables / "portrait_frame_data.txt", [[i, 1, 1, 1, 4] for i in range(1, 59) if i != 32])
    return root, base, out


def run(root, base, out):
    return subprocess.run([str(TOOL), str(root), str(base), str(out)], capture_output=True,
                          text=True, timeout=15, umask=0o077)


def test_binary_choices_localized_names_and_known_missing_expression(prepared):
    root, base, out = prepared
    before = {path: path.read_bytes() for folder in (root, base) for path in folder.rglob("*") if path.is_file()}
    result = run(root, base, out)
    assert result.returncode == 0, result.stderr
    report = json.loads((out / "fx_conversion.json").read_text())
    assert report["spell_bindings"] == 37 and report["face_slots"] == 110
    assert report["face_events"] == 101 and report["expression_choices"] == 146
    assert report["no_expression_ids"] == [0, 33, 35, 90, 94, 95, 102, 109]
    assert report["unsupported_face_event_ids"] == [64]
    assert report["typed_validation"] and not report["executable_executed"]
    assert not report["runtime_ready"] and report["portrait_fx_events_ready"]
    assert report["portrait_event_kinds"] == 7 and report["event_binding_checks"] == 28
    assert report["event_operation_order_validated"] and not report["automatic_face_reactions_validated"]
    events = {r[0]: r for r in read(out / "engine/data_tables/portrait_fx_events.txt")}
    assert len(events) == 7
    assert events["StatIncrease"][2:] == ["spboost2", "", "spboost1", "spboost2", "spboost3"]
    assert events["StatDecrease"][2:] == ["spboost3", "", "spboost1", "spboost2", "spboost3"]
    assert events["MonsterSpecial"][2:] == ["zapp", "", "", "", ""]
    spells = {int(row[0]): row for row in read(out / "engine/data_tables/spell_fx.txt")}
    assert spells[3] == ["3", "魔法3", "spell03"]
    assert spells[8][-1] == spells[3][-1] and spells[27][-1] == "spboost2"
    assert all(i not in spells for i in (0, 1, 21, 96, 109, 150, 151, 152, 153))
    faces = {int(row[0]): row for row in read(out / "engine/data_tables/face_animations.txt")}
    raw = (root / "mm8.exe").read_bytes()
    for i in range(110):
        choices = [v for v in raw[0xfd660 + i * 8 + 3:0xfd660 + i * 8 + 8] if v]
        if not choices or i == 64:
            assert i not in faces
        else:
            assert list(map(int, faces[i][2].split(","))) == choices
    assert faces[49][1] == "CastSpell" and faces[62][1] == "62"
    assert all(path.read_bytes() == content for path, content in before.items())
    assert all(path.stat().st_mode & 0o777 == 0o644 for path in out.rglob("*") if path.is_file())


@pytest.mark.parametrize("fault", ["binary_changed", "binary_truncated", "missing_exe", "face_override",
    "enum_duplicate", "enum_wrong_identity", "enum_expression", "missing_spell", "missing_icon",
    "zero_icon_time", "missing_portrait", "missing_generic_animation", "bad_quoting"])
def test_unsupported_profile_or_broken_associations_produce_no_output(prepared, fault):
    root, base, out = prepared
    table = base / "engine/data_tables"
    enum = root / "Scripts/Core/ConstAndBits.lua"
    if fault.startswith("binary_"):
        blob = bytearray((root / "mm8.exe").read_bytes())
        if fault == "binary_changed":
            blob[0xfd660 + 8 + 3] ^= 1
        else:
            del blob[-1:]
        (root / "mm8.exe").write_bytes(blob)
    elif fault == "missing_exe":
        (root / "mm8.exe").unlink()
    elif fault == "face_override":
        write(root / "Data/Tables/Face Animations.txt", [["#", "Expressions", "Sounds"], [1, 37, 38]])
    elif fault == "enum_duplicate":
        enum.write_text(enum.read_text().replace("LookDown = 64", "LookDown = 49"))
    elif fault == "enum_wrong_identity":
        enum.write_text(enum.read_text().replace("CastSpell", "SomethingElse"))
    elif fault == "enum_expression":
        enum.write_text(enum.read_text().replace("49,", "48 + 1,"))
    elif fault == "missing_spell":
        write(table / "spells.txt", [row for row in read(table / "spells.txt") if row[0] != "3"])
    elif fault == "missing_icon":
        write(table / "icon_frame_data.txt", [row for row in read(table / "icon_frame_data.txt") if row[0] != "spheal3"])
    elif fault == "zero_icon_time":
        rows = read(table / "icon_frame_data.txt")
        rows[0][2:4] = ["0", "0"]
        write(table / "icon_frame_data.txt", rows)
    elif fault == "missing_portrait":
        write(table / "portrait_frame_data.txt", [row for row in read(table / "portrait_frame_data.txt") if row[0] != "37"])
    elif fault == "missing_generic_animation":
        write(table / "icon_frame_data.txt", [row for row in read(table / "icon_frame_data.txt") if row[0] != "zapp"])
    else:
        (table / "spells.txt").write_text('"unclosed')
    result = run(root, base, out)
    assert result.returncode == 1 and "FX conversion failed" in result.stderr
    assert not out.exists()


@pytest.mark.parametrize("which", ["binary", "enum", "base_table"])
def test_symlink_escape_rejected(prepared, which):
    root, base, out = prepared
    path = {"binary": root / "mm8.exe", "enum": root / "Scripts/Core/ConstAndBits.lua",
            "base_table": base / "engine/data_tables/spells.txt"}[which]
    external = root.parent / "external"
    content = path.read_bytes()
    external.write_bytes(content)
    path.unlink()
    path.symlink_to(external)
    assert run(root, base, out).returncode == 1
    assert not out.exists() and external.read_bytes() == content


def test_existing_and_input_overlapping_outputs_rejected(prepared):
    root, base, out = prepared
    out.mkdir()
    (out / "keep").write_text("preserve")
    for dest in (out, root / "child", base / "child"):
        assert run(root, base, dest).returncode == 1
    assert (out / "keep").read_text() == "preserve"
    assert not (root / "child").exists() and not (base / "child").exists()
