"""Positional voice sounds, duplicate weighting, availability and source boundaries."""
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_bolster_resources import source_rows, write_source
from test_doll_resources import read, tables
from test_roster_resources import write

TOOL = Path(os.environ.get("VOICE_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/voice_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build voice_import first")
SOUND_TYPES = ("Disarmed Disarmed Closed1 Closed2 FailedDisarm1 FailedDisarm2 ChooseMe1 ChooseMe2 "
               "BadItem1 BadItem2 GoodItem1 GoodItem2 CantIdent1 CantIdent2 Repaired1 Repaired2 CantRep1 CantRep2 "
               "WeakMonster1 WeakMonster2 BigMonster1 BigMonster2 CantIdMon1 CantIdMon2 QuickSpell1 QuickSpell2 "
               "Hungry1 Hungry2 SoftInjure1 SoftInjure1 HardInjured1 HardInjured2 Dying1 Dying2 Drunk1 Drunk2 "
               "Insane1 Insane2 Poison1 Poison2 Cursed1 Cursed2 Fall1 Fall2 CantRestHere1 CantRestHere2 "
               "NeedMoreGold1 NeedMoreGold2 InventoryFull1 InventoryFull2 PotionMixed1 PotionMixed2 FailMixing1 "
               "FailMixing2 NeedAKey1 NeedAKey2 LearnSpell1 LearnSpell2 CantLearn1 CantLearn2 CantEquip1 CantEquip2 "
               "Hello1 Hello2 Hello3 Hello4 Hello5 Hello6 n/uCurse1 n/uCurse2 n/uLaughter1 n/uLaughter2 "
               "Win!1 Win!2 Heh1 Heh2 LastStanding1 LastStanding2 Leave1 Leave2 Enter1 Enter2 Yes1 Yes2 Thanks1 "
               "Thanks2 Indignant Indignant n/uObligatory n/uObligatory n/uShutUp1 n/uShutUp2 Move!1 Move!2 "
               "n/u n/u n/u n/u n/u n/u").split()
CATEGORIES = ["Man", "Woman", "Dragon", "Undead Man", "Undead Woman", "Undead Dragon"]


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    rows = [["Soundtype/Voicesetid", *map(str, range(6))]]
    for index, name in enumerate(SOUND_TYPES):
        rows.append([name] + [str(index * 6 + voice + 1) for voice in range(6)])
    rows[2][1] = rows[1][1]  # Same reaction and same sound ID must retain double weight.
    rows[3][1] = "0"  # Zero is a silent slot, never a sound ID to play.
    rows[100][6] = "65535"
    rows.append([""] * 7)
    for category, name in enumerate(CATEGORIES):
        rows.append([name] + ["x" if voice == category else "-" for voice in range(6)])
    rows.extend([[""] * 7, ["Notes", 'Literal "source" note', "", "", "", "", ""]])
    write_source(root / "Data/Tables/Character voices.txt", rows)
    portraits, _ = tables()
    portraits = portraits[:2]
    portraits[1][3] = "0"
    write(assets / "engine/data_tables/character_data.txt", portraits)
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def test_positional_sounds_duplicates_zero_flags_and_notes(prepared):
    root, assets, output = prepared
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    rows = read(output / "engine/data_tables/character_voices.txt")
    assert rows[0] == ["#CharacterVoiceMode", "Complete"]
    assert rows[1:] == source_rows(root / "Data/Tables/Character voices.txt")
    report = json.loads((output / "voice_conversion.json").read_text())
    assert report["voice_sets"] == 6 and report["sound_slots_per_voice"] == 100
    assert report["zero_sound_slots"] == 1 and report["distinct_nonzero_sound_ids"] == 598
    assert report["reaction_query_checks"] == len(set(SOUND_TYPES)) * 6
    assert report["availability_by_category"] == [1] * 6 and report["availability_checks"] == 40
    assert report["portrait_default_voices_checked"] == 1
    assert report["source_portraits_with_default_outside_availability"] == []
    assert report["typed_validation"] and report["legacy_table_behavior_preserved"]
    assert report["audio_assets_exported"] == 0 and not report["voice_playback_validated"]
    assert not report["rendered_voice_selection_validated"] and not report["runtime_ready"]
    assert all(p.read_bytes() == data for p, data in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


def test_empty_availability_is_an_explicit_empty_set(prepared):
    path = prepared[0] / "Data/Tables/Character voices.txt"
    rows = source_rows(path)
    rows[102][1:] = ["-"] * 6
    write_source(path, rows)
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    report = json.loads((prepared[2] / "voice_conversion.json").read_text())
    assert report["availability_by_category"] == [0, 1, 1, 1, 1, 1]
    assert report["source_portraits_with_default_outside_availability"] == [0]


@pytest.mark.parametrize("fault", ["header", "voice_id", "negative_sound", "overflow_sound", "empty_sound",
                                    "sound_label", "separator", "category_order", "category_flag", "footer",
                                    "width", "slot_count"])
def test_invalid_source_is_not_published(prepared, fault):
    path = prepared[0] / "Data/Tables/Character voices.txt"
    rows = source_rows(path)
    if fault == "width":
        rows[50].pop()
    elif fault == "slot_count":
        del rows[100]
    else:
        row, column, value = {
            "header": (0, 0, "Voices"), "voice_id": (0, 2, "0"), "negative_sound": (1, 1, "-1"),
            "overflow_sound": (1, 1, "65536"), "empty_sound": (1, 1, ""),
            "sound_label": (100, 0, "Man"), "separator": (101, 1, "x"),
            "category_order": (102, 0, "Woman"), "category_flag": (102, 1, "X"),
            "footer": (109, 0, "Disarmed"),
        }[fault]
        rows[row][column] = value
    write_source(path, rows)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_portrait_default_outside_voice_array_is_rejected(prepared):
    path = prepared[1] / "engine/data_tables/character_data.txt"
    rows = read(path)
    rows[1][3] = "6"
    write(path, rows)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_output_and_symlink_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    path = root / "Data/Tables/Character voices.txt"
    outside = tmp_path / "outside.txt"
    path.rename(outside)
    path.symlink_to(outside)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()
