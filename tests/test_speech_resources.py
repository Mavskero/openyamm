"""Original owned binary with synthetic face, complete voice and sound prerequisites."""
import csv
from io import StringIO
import json
import os
from pathlib import Path
import re
import subprocess

import pytest

REPO = Path(__file__).parents[1]
BUILD = REPO / "build-apple"
TOOL = Path(os.environ.get("SPEECH_IMPORT_TOOL", BUILD / "speech_import"))
EXE = Path(os.environ.get("MM8_EXE_FIXTURE", "/Users/yukarii/Downloads/MM678_Merged_05.11.2023_gdrv/mm8.exe"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build speech_import first")


def write(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        csv.writer(stream, delimiter="\t", lineterminator="\n").writerows(rows)


def read(path):
    return list(csv.reader(StringIO(path.read_text()), delimiter="\t"))


@pytest.fixture
def prepared(tmp_path):
    if not EXE.is_file():
        pytest.skip("Supply an owned mm8.exe via MM8_EXE_FIXTURE")
    root, base, output = (tmp_path / name for name in ("source", "base", "output"))
    root.mkdir()
    binary = EXE.read_bytes()
    (root / "mm8.exe").write_bytes(binary)
    enum = (REPO / "game/tables/PortraitEnums.h").read_text().split("enum class FaceAnimationId")[1].split("};")[0]
    names = {}
    for name, value in re.findall(r"(\w+)\s*=\s*(\d+)", enum):
        names.setdefault(int(value), name)
    faces = []
    for id_, name in sorted(names.items()):
        expressions = [n for n in binary[0xFD660 + id_ * 8 + 3:0xFD660 + (id_ + 1) * 8] if n]
        if expressions:
            faces.append([str(id_), name, ",".join(map(str, expressions))])
    tables = base / "engine/data_tables"
    write(tables / "face_animations.txt", faces)
    voices = [["#CharacterVoiceMode", "Complete"], ["Soundtype/Voicesetid", "0", "1"]]
    for slot in range(100):
        label = ["Yes1", "Yes2", "Thanks1", "Thanks2", "Indignant", "Indignant"][slot - 82] \
            if 82 <= slot < 88 else "RepeatedLabel"
        voices.append([label, str(5000 + slot), str(6000 + slot)])
    for category in ["Man", "Woman", "Dragon", "Undead Man", "Undead Woman", "Undead Dragon"]:
        voices.append([category, "x", "-"])
    voices.append(["Notes", "", ""])
    write(tables / "character_voices.txt", voices)
    write(tables / "sounds.txt", [[f"sound{n}", str(n), "swap", "-"] for n in range(5000, 5100)])
    return root, base, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=20, umask=0o077)


def test_original_voice_groups_face_bindings_and_silent_events(prepared):
    root, base, output = prepared
    before = {p: p.read_bytes() for folder in (root, base) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    rows = read(output / "engine/data_tables/character_speech_events.txt")
    assert rows[0] == ["#SpeechReactionMode", "MM8VoiceGroupsV1"]
    events = {row[0]: row for row in rows[2:]}
    assert len(events) == 87
    binary = (root / "mm8.exe").read_bytes()
    for row in events.values():
        if not row[4]:
            assert row[0] in ("Yes", "ThankYou", "Indignant") and not row[3]
            assert row[2] == {"Yes": "42", "ThankYou": "43", "Indignant": "44"}[row[0]]
            continue
        source = int(row[4])
        data = binary[0xFD660 + source * 8:0xFD660 + (source + 1) * 8]
        assert row[2] == ",".join(str(n) for n in data[:2] if n)
        assert bool(row[3]) == any(data[3:])
    known = {"DisarmTrap": (4, "1"), "HelloDay": (22, "32,33"), "HelloHouse": (84, "32,33"),
             "DamageMinor": (24, "15"), "DamageMajor": (48, "16"), "Dying": (33, "17"),
             "KillWeakEnemy": (1, "38"), "KillStrongEnemy": (2, "37"), "SelectCharacter": (102, "4"),
             "CastSpell": (49, ""), "DeathBlow": (109, "17")}
    for name, (source, groups) in known.items():
        assert events[name][2] == groups and events[name][4] == str(source)
    assert not events["Dying"][3] and not events["SelectCharacter"][3] and not events["DeathBlow"][3]
    report = json.loads((output / "speech_conversion.json").read_text())
    assert report["events"] == 87 and report["voice_sets"] == 2 and report["direct_voice_events"] == 3
    assert report["face_bindings"] == sum(bool(row[3]) for row in events.values())
    assert report["voiced_events"] == sum(bool(row[2]) for row in events.values())
    used_groups = {int(g) for row in events.values() for g in row[2].split(",") if g}
    assert report["missing_sound_ids"] == sorted(6000 + (g - 1) * 2 + v for g in used_groups for v in (0, 1))
    assert report["typed_validation"] and not report["runtime_ready"]
    assert not report["audio_playback_validated"] and not report["rendered_reactions_validated"]
    assert not report["original_rng_parity"] and not report["patched_windows_parity"]
    assert all(p.read_bytes() == value for p, value in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["fingerprint", "missing_face", "wrong_expression", "wrong_face_name",
                                    "voice_count", "voice_value", "voice_width", "direct_pair_order", "empty_sounds"])
def test_invalid_speech_dependencies_do_not_publish(prepared, fault):
    root, base, output = prepared
    tables = base / "engine/data_tables"
    if fault == "fingerprint":
        binary = bytearray((root / "mm8.exe").read_bytes())
        binary[0xFD660 + 22 * 8] ^= 1
        (root / "mm8.exe").write_bytes(binary)
    elif fault in ("missing_face", "wrong_expression", "wrong_face_name"):
        path = tables / "face_animations.txt"
        rows = read(path)
        if fault == "missing_face":
            rows = [row for row in rows if row[0] != "22"]
        else:
            row = next(row for row in rows if row[0] == "22")
            row[2 if fault == "wrong_expression" else 1] = "25" if fault == "wrong_expression" else "CastSpell"
        write(path, rows)
    elif fault.startswith("voice_") or fault == "direct_pair_order":
        path = tables / "character_voices.txt"
        rows = read(path)
        if fault == "voice_count":
            rows.pop(20)
        elif fault == "voice_value":
            rows[20][1] = "bad"
        elif fault == "voice_width":
            rows[20].pop()
        else:
            rows[84][0] = "OtherGroup"
        write(path, rows)
    else:
        write(tables / "sounds.txt", [["Name", "Id"]])
    result = run(prepared)
    assert result.returncode != 0 and not output.exists(), result.stdout


@pytest.mark.parametrize("file", ["mm8.exe", "face_animations.txt", "character_voices.txt", "sounds.txt"])
def test_input_escape_is_rejected(prepared, tmp_path, file):
    root, base, output = prepared
    path = root / file if file == "mm8.exe" else base / "engine/data_tables" / file
    outside = tmp_path / "outside"
    path.rename(outside)
    path.symlink_to(outside)
    assert run(prepared).returncode != 0 and not output.exists()


def test_output_preservation(prepared):
    root, base, output = prepared
    for parent in (root, base):
        assert run((root, base, parent / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"


def test_shared_selection_preserves_group_probability():
    tool = BUILD / "speech_group_check"
    assert tool.is_file(), "Build speech_group_check"
    result = subprocess.run([str(tool)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
