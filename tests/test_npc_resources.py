"""NPC rules, localized profession identity and distinct news text registries."""
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_doll_resources import read
from test_roster_resources import write
from test_story_resources import write_archive

TOOL = Path(os.environ.get("NPC_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/npc_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build npc_import first")
PROF_HEADER = ["#", "Professions", "Prof name (Global.txt)", "Rarity", "Cost/w", "Personality",
               "Action Topic", "Joins", "Recruit", "Join Text", "Description"]
BTB_HEADER = ["", "Beg", "Bribe", "Threat", "Creed", "Fame required", "Reputation required",
              "Rep ok, 1st greet", "Rep ok, 2nd greet", "Beg return", "Bribe return", "Threat return", "Fame too low",
              "Rep notorious, I'm good", "Rep notorious, I'm evil", "Rep saintly, I'm good", "Rep Saintly, I'm evil",
              "Rep below zero, 1st greet", "Rep above 10, 1st greet (and I'm evil)",
              "You aren't good enough for me. (1st time)", "You aren't bad enough for me.",
              "Rep below zero, 2nd greet", "Rep above 10, 2nd greet (and I'm evil)",
              "You still aren't good enough for me (2nd time)", "You still aren't bad enough for me (2nd time)",
              "I accept your beg", "I don't like begging", "Thanks for the bribe", "I don't like bribes",
              "I accept your threat", "I don't like threats"]
TABLE_NAMES = {"NPC professions.txt": "npc_professions.txt", "NPC names.txt": "npc_names.txt",
               "NPC BTB.txt": "npc_btb.txt", "News topics - area.txt": "news_topics_area.txt",
               "News topics - continent.txt": "news_topics_continent.txt",
               "News topics - profession.txt": "news_topics_profession.txt"}


def globals_table(chinese=False):
    return [["Global Text", ""], [], ["308", "铁匠" if chinese else "Smith"],
            ["309", "贵族" if chinese else "Noble"]]


@pytest.fixture
def prepared(tmp_path):
    root, assets, output = tmp_path / "source", tmp_path / "assets", tmp_path / "converted"
    table_dir = root / "Data/Tables"
    write(table_dir / "NPC professions.txt", [PROF_HEADER,
          ["0", "", "", "100", "0", "Merchant", "", "", "", "1000", ""],
          ["1", "Smith", "308", "10", "200", "Merchant", "1002", "x", "", "1000", "1001"],
          ["2", "Noble", "309", "100", "0", "Noble", "", "x", "x", "1000", ""]])
    write(table_dir / "NPC names.txt", [["Male", "Female"], ["Aaron", "Alice"], ["Aaron"], ["Zed", "Beth"]])
    write(table_dir / "NPC BTB.txt", [BTB_HEADER,
          ["Merchant", "", "x", "", "Neutral", "0", "-5"] + [str(1000 + i) for i in range(24)],
          ["Noble", "x", "", "", "Light", "200", "10"] + [str(1000 + i) for i in range(24)]])
    write(table_dir / "News topics - area.txt", [["Area", "Topic Name", "Text"],
          ["1", "1", "1"], ["1", "NPCTopic 1002", "NPCText 1000"]])
    write(table_dir / "News topics - continent.txt", [["Continent", "Topic Name", "Text"], ["3", "2", "2"]])
    days = [f"{day} {kind}" for day in ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday")
            for kind in ("topic", "text")]
    weekly = [["#", "Profession name"] + days,
              ["1", "Smith"] + [str(i) for i in range(1, 8) for _ in (0, 1)],
              ["2", "Old profession note"] + [str(i) for i in range(1, 8) for _ in (0, 1)]]
    write(table_dir / "News topics - profession.txt", weekly)
    write(table_dir / "Continent settings.txt", [["#", "Notes"], ["3", "Enroth"]])
    write_archive(root / "Data/mmmerge.T.lod", {"global.txt": globals_table()})
    write_archive(root / "Data/10 LocZHCN.EnglishT.lod", {"global.txt": globals_table(True)}, True)
    base = assets / "engine/data_tables"
    write(base / "npc.txt", [["10", "训练师", "50", "0", "0", "0", "30", "1", "1", "0",
                             "1002", "0", "0", "0", "0", "0"]])
    write(base / "npc_topic.txt", [["1002", "职业能力", "0", "", "1000", "", ""]])
    write(base / "npc_topic_text.txt", [[str(1000 + i), f"中文对话{i}"] for i in range(24)])
    write(base / "npc_news.txt", [[str(i), f"中文传闻{i}", f"话题{i}"] for i in range(1, 8)])
    write(base / "map_stats.txt", [["1", "测试场景", "fixture.odm"] + ["0"] * 31 + ["mm6"]])
    return root, assets, output


def run(prepared):
    return subprocess.run([str(TOOL), *map(str, prepared)], capture_output=True, text=True, timeout=15, umask=0o077)


def test_source_cells_localized_names_weekdays_and_news_namespace(prepared):
    root, assets, output = prepared
    before = {p: p.read_bytes() for folder in (root, assets) for p in folder.rglob("*") if p.is_file()}
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    for source, target in TABLE_NAMES.items():
        rows = read(output / "engine/data_tables" / target)
        expected = read(root / "Data/Tables" / source)
        if source == "NPC professions.txt":
            assert [r[-1] for r in rows] == ["Display Name", "", "铁匠", "贵族"]
            rows = [r[:-1] for r in rows]
        assert rows == expected
    report = json.loads((output / "npc_conversion.json").read_text())
    assert report["professions"] == 3 and report["translated_professions"] == 2
    assert report["male_names"] == 3 and report["female_names"] == 2
    assert report["profession_day_news"] == 14 and report["profession_news_label_differences"] == 1
    assert report["explicit_npc_text_references"] == 1 and report["namespace_dialog_check"]
    assert report["follower_display_check"]
    assert not report["runtime_ready"] and not report["btb_gameplay_validated"]
    assert not report["random_names_localized"] and not report["original_news_selection_parity"]
    assert all(p.read_bytes() == data for p, data in before.items())
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


def test_english_without_localization(prepared):
    (prepared[0] / "Data/10 LocZHCN.EnglishT.lod").unlink()
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    rows = read(prepared[2] / "engine/data_tables/npc_professions.txt")
    assert rows[2][-1] == "Smith" and rows[3][-1] == "Noble"


@pytest.mark.parametrize("fault", ["profession_order", "weekly_order", "personality", "marker", "cost", "global_id",
                                    "btb_text", "map", "continent", "news_title", "news_text", "namespace", "name_width"])
def test_invalid_mechanics_or_reference_has_no_output(prepared, fault):
    root, _, output = prepared
    filename, row, col, value = {
        "profession_order": ("NPC professions.txt", 2, 0, "2"),
        "weekly_order": ("News topics - profession.txt", 1, 0, "2"),
        "personality": ("NPC professions.txt", 2, 5, "Unknown"),
        "marker": ("NPC professions.txt", 2, 7, "yes"),
        "cost": ("NPC professions.txt", 2, 4, "-1"),
        "global_id": ("NPC professions.txt", 2, 2, "309"),
        "btb_text": ("NPC BTB.txt", 1, 7, "999"),
        "map": ("News topics - area.txt", 1, 0, "999"),
        "continent": ("News topics - continent.txt", 1, 0, "999"),
        "news_title": ("News topics - area.txt", 1, 1, "1002"),
        "news_text": ("News topics - area.txt", 1, 2, "1000"),
        "namespace": ("News topics - area.txt", 2, 2, "OtherText 1000"),
        "name_width": ("NPC names.txt", 1, 0, "Aaron"),
    }[fault]
    path = root / "Data/Tables" / filename
    rows = read(path)
    rows[row][col] = value
    if fault == "name_width":
        rows[row].append("extra")
    write(path, rows)
    assert run(prepared).returncode != 0 and not output.exists()


def test_output_and_symlink_boundaries(prepared, tmp_path):
    root, assets, output = prepared
    assert run((root, assets, root / "nested")).returncode != 0
    assert run((root, assets, assets / "nested")).returncode != 0
    output.mkdir()
    (output / "keep").write_text("unchanged")
    assert run(prepared).returncode != 0 and (output / "keep").read_text() == "unchanged"
    archive = root / "Data/mmmerge.T.lod"
    outside = tmp_path / "outside.lod"
    archive.rename(outside)
    archive.symlink_to(outside)
    fresh = tmp_path / "fresh"
    assert run((root, assets, fresh)).returncode != 0 and not fresh.exists()
