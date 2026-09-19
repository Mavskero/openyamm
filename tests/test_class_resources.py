"""Native positional class conversion; synthetic data only, no game or device required."""
import csv
from io import StringIO
import json
import os
from pathlib import Path
import subprocess

import pytest

TOOL = Path(os.environ.get("CLASS_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/class_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build class_import first")

CLASSES = ("Archer,WarriorMage,MasterArcher,Sniper,Cleric,Priest,PriestLight,PriestDark,"
           "DarkElf,Patriarch,Dragon,GreatWyrm,Druid,GreatDruid,Warlock,ArchDruid,Knight,"
           "Cavalier,BlackKnight,Champion,Minotaur,MinotaurLord,Monk,Initiate,Master,Ninja,"
           "Paladin,Crusader,Hero,Villain,Ranger,Hunter,BountyHunter,RangerLord,Thief,Rogue,"
           "Assassin,Spy,Troll,WarTroll,Vampire,Nosferatu,Sorcerer,Wizard,Necromancer,Lich,"
           "ArchMage,DarkArchMage,Peasant,n/u,HighPriest,MasterWizard,n/u").split(",")
SKILLS = ("Staff Sword Dagger Axe Spear Bow Mace Blaster Shield Leather Chain Plate Fire Air Water Earth "
          "Spirit Mind Body Light Dark DarkElf Vampire Dragon IdentifyItem Merchant Repair Bodybuilding "
          "Meditation Perception Regeneration Disarm Dodging Unarmed IdentifyMonster Armsmaster Stealing "
          "Alchemy Learning").split()
GROUPS = ("Archer,WarriorMage,Cleric,Priest,DarkElf,Dragon,Druid,GreatDruid,Knight,Cavalier,"
          "Minotaur,Monk,Initiate,Paladin,Crusader,Ranger,Hunter,Thief,Rogue,Troll,Vampire,"
          "Sorcerer,Necromancer,n/u,Peasant,High Priest / Master Wizard,n/u,n/u,n/u,n/u").split(",")
KINDS = [1]*4 + [2]*4 + [3]*2 + [4]*2 + [5]*4 + [6]*4 + [7]*2 + [8]*4 + [9]*4 + [10]*4 + [11]*4
KINDS += [12]*2 + [13]*2 + [14]*2 + [15]*2 + [14]*2 + [16, 0, 2, 14, 0]
STEPS = [0, 1, 2, 2]*2 + [1, 2]*2 + [0, 1, 2, 2]*2 + [1, 2] + [0, 1, 2, 2]*4
STEPS += [1, 2]*2 + [0, 1, 1, 2, 2, 2, 0, 0, 2, 2, 0]
FILES = ["Class HP SP.txt", "Class Extra.txt", "Class Skills.txt", "Class Starting Skills.txt"]
ACTIVE = [i for i in range(53) if i not in (49, 52)]


def encode(rows):
    out = StringIO()
    csv.writer(out, delimiter="\t", lineterminator="\r\n").writerows(rows)
    return out.getvalue().encode("cp1252")


def source(root, mutate=None):
    hp = [["", "HPBase", "HPFactor", "SPBase", "SPFactor", "SPStats"]]
    extra = [["#", "Kind", "Step", "Note"]]
    for id_, name in enumerate(CLASSES):
        mode = ["-", "I", "P", "IP", "PI"][id_ % 5]
        hp.append([name, str(20+id_), str(id_ % 8+1), str(id_ % 21), str(id_ % 7), mode])
        extra.append([str(id_), str(KINDS[id_]), str(STEPS[id_]), name])
    caps = [[""]+CLASSES]
    starting = [[""]+GROUPS]
    for sk, name in enumerate(SKILLS):
        caps.append([name]+[["-", "B", "E", "M", "G"][(sk+id_) % 5] for id_ in range(53)])
        starting.append([name]+[["-", "C", "F"][(sk+group) % 3] for group in range(30)])
    tables = dict(zip(FILES, [hp, extra, caps, starting]))
    if mutate:
        mutate(tables)
    directory = root / "Data/Tables"
    directory.mkdir(parents=True)
    for name, rows in tables.items():
        (directory/name).write_bytes(encode(rows))
    return tables


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True,
                          timeout=15, umask=0o077)


def read(path):
    with path.open(newline="") as stream:
        return list(csv.reader(stream, delimiter="\t"))


def test_all_classes_keep_ids_mechanics_and_positional_starting_groups(tmp_path):
    root, output = tmp_path/"source", tmp_path/"output"
    src = source(root)
    before = {p: p.read_bytes() for p in root.rglob("*") if p.is_file()}
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    table = output/"engine/data_tables"
    hp = read(table/"class_multipliers.txt")
    extra = read(table/"class_extra.txt")
    caps = read(table/"class_skills.txt")
    initial = read(table/"class_starting_skills.txt")
    modes = {"-": "None", "I": "Intellect", "P": "Personality", "IP": "Mixed", "PI": "Mixed"}
    assert len(hp) == len(extra) == len(initial) == 52
    assert len(caps) == 40 and all(len(row) == 52 for row in caps)
    for row, id_ in enumerate(ACTIVE, 1):
        assert extra[row] == src[FILES[1]][id_+1]
        original = src[FILES[0]][id_+1]
        assert hp[row] == original[:5] + [modes[original[5]]]
        assert initial[row][0] == CLASSES[id_]
        for sk in range(39):
            assert caps[sk+1][row] == src[FILES[2]][sk+1][id_+1]
            assert initial[row][sk+1] == src[FILES[3]][sk+1][id_//2+1]
    # Distinguish floor(ID/2) from both header lookup and promotion Kind lookup.
    assert initial[2][1:] == initial[1][1:]  # Archer / WarriorMage share group 0.
    assert initial[3][1:] != initial[1][1:]
    assert extra[-2][0] == "50" and extra[-1][0] == "51"
    assert initial[-2][1:] == initial[-1][1:]  # Shared High Priest / Master Wizard source group.
    report = json.loads((output/"class_conversion.json").read_text())
    assert report["active_classes"] == 51 and report["inactive_class_ids"] == [49, 52]
    assert report["unused_starting_group_indices"] == [26, 27, 28, 29]
    assert report["default_skill_assignments"] == 51*13
    assert report["starting_group_by_class"] == [{"class_id": i, "group_index": i//2} for i in ACTIVE]
    assert report["typed_validation"] and report["promotion_validation"] and report["hp_sp_progression_validated"]
    assert not report["runtime_ready"] and not report["race_overrides_ready"]
    assert report["starting_without_class_cap"]  # Preserved source conflicts, no invented caps.
    assert before == {p: p.read_bytes() for p in root.rglob("*") if p.is_file()}
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["id", "name", "reserved", "negative", "overflow", "junk_number", "mode",
                                "mastery", "availability", "skill_order", "starting_header", "width", "rows",
                                "promotion_cycle"])
def test_invalid_source_is_rejected_without_output(tmp_path, fault):
    def mutate(t):
        hp, extra, caps, start = [t[n] for n in FILES]
        if fault == "id": extra[51][0] = "49"
        if fault == "name": hp[1][0] = "Cleric"
        if fault == "reserved": extra[50][1] = "17"
        if fault == "negative": hp[1][1] = "-1"
        if fault == "overflow": hp[1][1] = "256"
        if fault == "junk_number": hp[1][1] = "30bad"
        if fault == "mode": hp[1][5] = "L"
        if fault == "mastery": caps[1][1] = "GM"
        if fault == "availability": start[1][1] = "B"
        if fault == "skill_order": caps[1][0] = "Sword"
        if fault == "starting_header": start[0][1], start[0][2] = start[0][2], start[0][1]
        if fault == "width": caps[2].pop()
        if fault == "rows": extra.pop(50)
        if fault == "promotion_cycle": extra[1][2] = "2"
    root, output = tmp_path/"source", tmp_path/"output"
    source(root, mutate)
    result = run(root, output)
    assert result.returncode == 1 and "Class conversion failed" in result.stderr
    assert not output.exists()


def test_output_and_source_boundaries(tmp_path):
    root, output = tmp_path/"source", tmp_path/"output"
    source(root)
    output.mkdir()
    (output/"keep").write_text("unchanged")
    assert run(root, output).returncode == 1
    assert (output/"keep").read_text() == "unchanged"
    assert run(root, root/"nested").returncode == 1
    assert not (root/"nested").exists()
    assert run(root, tmp_path).returncode == 1
    alias = tmp_path/"alias"
    alias.symlink_to(root, target_is_directory=True)
    assert run(root, alias/"nested").returncode == 1
    victim = root/"Data/Tables/Class Extra.txt"
    escaped = tmp_path/"outside.txt"
    victim.rename(escaped)
    victim.symlink_to(escaped)
    result = run(root, tmp_path/"escape-output")
    assert result.returncode == 1 and "escapes root" in result.stderr
    assert not (tmp_path/"escape-output").exists()
