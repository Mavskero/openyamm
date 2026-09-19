"""Teacher definitions, paid training and Chinese NPC/journal associations."""
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_roster_resources import write
from test_doll_resources import read

TOOL = Path(os.environ.get("TEACHER_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/teacher_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build teacher_import first")


def tables():
    topics = [["TopicId", "Notes", "SkillId", "Mastery", "TextId", "Req. Gold", "Req. Skill"],
              ["300", "Expert Staff ", "0", "1", "300", "", ""],
              ["971", "Expert Blaster", "7", "1", "1295", "2000", "4"],
              ["972", "Master Blaster", "7", "2", "1296", "5000", "7"],
              ["973", "Grand Master Blaster", "7", "3", "1297", "10000", "10"]]
    notes = [["Topic Id", "Notes", "NPC Id", "Autonote Id", "NPC", "A", "NPC", "A"],
             ["300", "Expert Staff ", "10", "1", "", ""], ["971", "Expert Blaster", "11", "2"]]
    return topics, notes


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    topics, notes = tables()
    write(root / "Data/Tables/Teacher topics.txt", topics)
    write(root / "Data/Tables/Teacher autonotes.txt", notes)
    base = assets / "engine/data_tables"
    write(base / "npc.txt", [[str(i), "训练师", "50", "0", "0", "0", "30", "9", "1", "0",
                              "300", "0", "0", "0", "0", "0"] for i in (10, 11)])
    write(base / "npc_topic.txt", [[row[0], "技能训练", "0", "", row[4], "Trainer", ""] for row in topics[1:]])
    write(base / "npc_topic_text.txt", [[row[4], "中文训练说明"] for row in topics[1:]])
    write(base / "english/autonote.txt", [[str(i), "训练师位置笔记", "Trainer", ""] for i in (1, 2)])
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def test_defaults_paid_training_explicit_overrides_and_bilingual_links(prepared):
    root, assets, output = prepared
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    topics, notes = tables()
    assert read(output / "engine/data_tables/teacher_topics.txt") == [["#TeacherTopicMode", "Complete"]] + topics
    assert read(output / "engine/data_tables/teacher_autonotes.txt") == notes
    report = json.loads((output / "teacher_conversion.json").read_text())
    assert report["topics"] == 4 and report["note_bindings"] == 2 and report["empty_note_pairs"] == 1
    assert report["price_overrides"] == report["skill_overrides"] == 3
    assert report["typed_validation"] and report["training_transaction_check"]
    assert report["complete_definitions"] and report["inactive_note_bindings"] == 0
    assert not report["runtime_ready"] and not report["original_price_parity_validated"]
    assert not report["rendered_training_validated"] and not report["teacher_note_event_validated"]
    assert all(p.read_bytes() == content for p, content in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["header", "width", "duplicate", "skill", "mastery", "gold_zero", "gold_negative",
                                    "level_zero", "topic_missing", "text_missing"])
def test_invalid_teacher_definitions_have_no_output(prepared, fault):
    topics, _ = tables()
    if fault == "header":
        topics[0][3] = "Different mastery encoding"
    elif fault == "width":
        topics[1].pop()
    elif fault == "duplicate":
        topics.append(topics[1].copy())
    else:
        column, value = {"skill": (2, "39"), "mastery": (3, "0"), "gold_zero": (5, "0"),
                         "gold_negative": (5, "-1"), "level_zero": (6, "0"),
                         "topic_missing": (0, "999"), "text_missing": (4, "999")}[fault]
        topics[1][column] = value
    write(prepared[0] / "Data/Tables/Teacher topics.txt", topics)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


@pytest.mark.parametrize("fault", ["half_pair", "pair_width", "npc_missing", "note_missing", "duplicate_binding",
                                    "duplicate_topic", "topic_missing"])
def test_invalid_teacher_notes_have_no_output(prepared, fault):
    _, notes = tables()
    if fault == "half_pair":
        notes[1][4] = "11"
    elif fault == "pair_width":
        notes[1].pop()
    elif fault == "npc_missing":
        notes[1][2] = "99"
    elif fault == "note_missing":
        notes[1][3] = "99"
    elif fault == "duplicate_binding":
        notes[1][4:6] = ["10", "2"]
    elif fault == "duplicate_topic":
        notes.append(notes[1].copy())
    else:
        notes[1][0] = "999"
    write(prepared[0] / "Data/Tables/Teacher autonotes.txt", notes)
    assert run(prepared).returncode != 0 and not prepared[2].exists()


def test_crlf_notes_and_preserved_multiline_comment(prepared):
    root, _, output = prepared
    topics, _ = tables()
    topics[1][1] = 'Staff "teacher"\nSource note'
    write(root / "Data/Tables/Teacher topics.txt", topics)
    for path in (root / "Data/Tables").iterdir():
        path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    assert read(output / "engine/data_tables/teacher_topics.txt")[2][1] == topics[1][1]


def test_complete_source_keeps_obsolete_notes_without_reactivating_training(prepared):
    root, assets, output = prepared
    _, notes = tables()
    notes.append(["321", "Retired blaster topic", "10", "2"])
    notes.append(["322", "Retired empty topic", "", ""])
    write(root / "Data/Tables/Teacher autonotes.txt", notes)
    path = assets / "engine/data_tables/npc_topic.txt"
    topics = read(path)
    topics += [[str(i), "旧话题", "0", "", "300", "Trainer", ""] for i in (321, 322)]
    write(path, topics)
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    assert read(output / "engine/data_tables/teacher_autonotes.txt") == notes
    report = json.loads((output / "teacher_conversion.json").read_text())
    assert report["inactive_note_topic_ids"] == [321, 322] and report["inactive_note_bindings"] == 1
    assert report["empty_note_pairs"] == 2 and report["topics"] == 4


def test_output_and_symlink_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    source = root / "Data/Tables/Teacher topics.txt"
    outside = tmp_path / "outside.txt"
    source.rename(outside)
    source.symlink_to(outside)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()
