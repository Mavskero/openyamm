"""Bilingual inspection tables and shared semantic lookup, with synthetic archives."""
import csv
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_class_resources import CLASSES, KINDS, STEPS, SKILLS
from test_story_resources import text, write_archive

TOOL = Path(os.environ.get("INSPECTION_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/inspection_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build inspection_import first")
STATS = ("Might,Intellect,Personality,Endurance,Accuracy,Speed,Luck,Hit Points,Armor Class,Spell Points,"
         "Condition,Quick Spell,Age,Level,Experience,Attack Bonus,Attack Damage,Shoot Bonus,Shoot Damage,"
         "Fire,Air,Water,Earth,Mind,Body,Skill Points").split(",")
ZH_STATS = ("力量 智力 个性 耐力 精确 速度 运气 生命值 防御等级 魔法值 状态 备用魔法 年龄 等级 经验值 "
            "攻击点数 攻击杀伤力 射击点数 射击杀伤力 火系抗性 风系抗性 水系抗性 土系抗性 心系抗性 体系抗性 技能点数").split()
ZH_SKILLS = ("手杖 长剑 匕首 战斧 长矛 弓箭 钉锤 榴弹枪 盾牌 皮甲 锁甲 钢甲 火系魔法 空气魔法 水系魔法 土系魔法 "
             "灵魂魔法 心智魔法 肢体魔法 光明魔法 暗黑魔法 黑暗精灵能力 吸血鬼能力 飞龙能力 物品鉴定术 经商术 修补术 "
             "健身术 冥想术 洞察术 再生术 陷阱拆卸术 闪躲术 空手搏斗术 怪物鉴定术 武器使用术 偷窃术 炼金术 学习术").split()
ZH_CLASSES = ("弓箭手 高级射手 神箭手 狙击手 牧师 神甫 光明主教 黑暗主教 黑暗精灵 暗精尊者 飞龙 神龙 德鲁伊 高级德鲁伊 "
              "邪术师 大德鲁伊 骑士 豪侠 黑骑士 勇士 牛头人 牛头领主 僧侣 沙弥 智者 忍者 圣武士 十字军 英雄 魔头 "
              "游骑兵 猎手 赏金猎人 游骑领主 盗贼 神偷 杀手 密探 巨魔 战斗巨魔 吸血鬼 吸血圣王 巫师 法师 死灵法师 "
              "巫妖 大法师 死灵大法师 农民 n/u 大主教 魔法大师 n/u").split()


def source(root, chinese=True, unquoted=False, fault=None):
    data = root/"Data"
    (data/"Tables").mkdir(parents=True)
    base = {
        "stats.txt": [["Stats Descriptions", "Description"]] + [[s, f"Base stat {i}"] for i, s in enumerate(STATS)],
        "skilldes.txt": [["Skill", "Description", "Normal", "Expert", "Master", "GrandMaster"]]
            + [[s]+[f"Base skill {i} field {j}" for j in range(5)] for i, s in enumerate(SKILLS)],
        "class.txt": [["Class name", "Class description", "Class kind"]]
            + [[("Master Necromancer" if i == 47 else s), f"Base class {i}", ("Wiz" if i == 47 else str(KINDS[i]))]
               for i, s in enumerate(CLASSES)],
    }
    metadata = [["#", "Kind", "Step", "Note"]]
    metadata += [[str(i), str(KINDS[i]), str(STEPS[i]), s] for i, s in enumerate(CLASSES)]
    locale = {k: [r[:] for r in v] for k, v in base.items()}
    for name, labels in [("stats.txt", ZH_STATS), ("skilldes.txt", ZH_SKILLS), ("class.txt", ZH_CLASSES)]:
        for i, label in enumerate(labels, 1):
            row = locale[name][i]
            row[0] = label
            for col in range(1, 6 if name == "skilldes.txt" else 2):
                row[col] = f'中文说明{i}列{col}，"引文"%31'
    locale["stats.txt"][1][1] = "力量\x0e \x0e说\x07\x0f明"
    locale["class.txt"][49][1] = "第一段\n\n第二段\n最后一段"
    if fault == "stat_order": base["stats.txt"][1], base["stats.txt"][2] = base["stats.txt"][2], base["stats.txt"][1]
    if fault == "skill_order": base["skilldes.txt"][1][0] = "Sword"
    if fault == "class_alias": base["class.txt"][48][0] = "Unrelated class"
    if fault == "class_id": metadata[51][0] = "49"
    if fault == "metadata_name": metadata[1][3] = "Cleric"
    if fault == "inactive": metadata[50][1] = "1"
    if fault == "blank": locale["skilldes.txt"][1][5] = ""
    if fault == "locale_count": locale["stats.txt"].pop()
    if fault == "locale_order":
        locale["class.txt"][1], locale["class.txt"][2] = locale["class.txt"][2], locale["class.txt"][1]
    if fault == "skill_locale_order":
        locale["skilldes.txt"][1], locale["skilldes.txt"][2] = locale["skilldes.txt"][2], locale["skilldes.txt"][1]
    if fault == "locale_kind": locale["class.txt"][1][2] = "Other family"
    if fault == "header": base["stats.txt"][0][0] = "New schema"
    (data/"Tables/Class Extra.txt").write_bytes(text(metadata).encode("cp1252"))
    write_archive(data/"mmmerge.T.lod", base)
    if chinese:
        raw = {}
        if unquoted:
            raw["class.txt"] = text(locale["class.txt"][:49]) + "农民\t第一段\n\n第二段\n最后一段\t16\n"
            raw["class.txt"] += text(locale["class.txt"][50:])
        if fault == "interrupted":
            raw["class.txt"] = text(locale["class.txt"][:49]) + "农民\t未完成\n" + text(locale["class.txt"][50:])
        if fault == "stray": raw["class.txt"] = text(locale["class.txt"]) + "多余一行\n"
        if fault == "unfinished": raw["class.txt"] = text(locale["class.txt"][:-1]) + "n/u\t未完成\n"
        write_archive(data/"10 LocZHCN.EnglishT.lod", locale, True, raw_texts=raw)
    return base, locale


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True, timeout=15, umask=0o077)


def read(path):
    with path.open(newline="") as f:
        return list(csv.reader(f, delimiter="\t"))


@pytest.mark.parametrize("chinese", [True, False])
def test_semantic_keys_display_names_descriptions_and_original_ids(tmp_path, chinese):
    root, output = tmp_path/"source", tmp_path/"output"
    base, locale = source(root, chinese=chinese)
    before = {p: p.read_bytes() for p in root.rglob("*") if p.is_file()}
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    table = output/"engine/data_tables/english"
    stats, skills, classes = [read(table/n) for n in ["stats.txt", "skill_des.txt", "class.txt"]]
    selected = locale if chinese else base
    assert len(stats) == 27 and len(skills) == 40 and len(classes) == 52
    assert stats[1] == ["Might", "力量说明" if chinese else "Base stat 0", "力量" if chinese else "Might"]
    for i in range(2,27):
        assert stats[i] == [STATS[i-1], selected["stats.txt"][i][1], selected["stats.txt"][i][0]]
    for i in range(1,40):
        assert skills[i] == [SKILLS[i-1]] + selected["skilldes.txt"][i][1:] + [selected["skilldes.txt"][i][0]]
    for row in classes[1:]:
        id_ = int(row[4])
        assert id_ not in [49,52] and row[0] == CLASSES[id_]
        assert row[1:4] == [selected["class.txt"][id_+1][1], base["class.txt"][id_+1][2],
                            selected["class.txt"][id_+1][0]]
    by_id = {int(r[4]):r for r in classes[1:]}
    assert by_id[47][0] == "DarkArchMage"
    assert by_id[47][3] == ("死灵大法师" if chinese else "Master Necromancer")
    assert by_id[50][0] == "HighPriest" and by_id[51][0] == "MasterWizard"
    report = json.loads((output/"inspection_conversion.json").read_text())
    assert report["localized_entries"] == (116 if chinese else 0)
    assert report["restored_glyphs"] == (1 if chinese else 0)
    assert report["typed_validation"] and report["legacy_tables_validated"]
    assert not report["full_gameplay_ui"] and not report["runtime_ready"]
    assert before == {p:p.read_bytes() for p in root.rglob("*") if p.is_file()}
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


def test_unquoted_class_paragraph_preserves_following_ids(tmp_path):
    outputs = []
    for unquoted in [False, True]:
        root = tmp_path/str(unquoted)/"source"
        output = root.parent/"output"
        source(root, unquoted=unquoted)
        result = run(root, output)
        assert result.returncode == 0, result.stderr
        outputs.append(output)
    for name in ["stats.txt", "skill_des.txt", "class.txt"]:
        assert (outputs[0]/"engine/data_tables/english"/name).read_bytes() == (
            outputs[1]/"engine/data_tables/english"/name).read_bytes()
    assert json.loads((outputs[1]/"inspection_conversion.json").read_text())["unquoted_continuation_lines"] == 3


@pytest.mark.parametrize("fault", ["stat_order", "skill_order", "class_alias", "class_id", "metadata_name",
    "inactive", "blank", "locale_count", "locale_order", "skill_locale_order", "locale_kind", "header",
    "interrupted", "stray", "unfinished"])
def test_malformed_or_shifted_tables_leave_no_output(tmp_path, fault):
    root, output = tmp_path/"source", tmp_path/"output"
    source(root, fault=fault)
    result = run(root, output)
    assert result.returncode == 1 and "Inspection conversion failed" in result.stderr
    assert not output.exists()


@pytest.mark.parametrize("escaped_name", ["mmmerge.T.lod", "10 LocZHCN.EnglishT.lod", "Tables/Class Extra.txt"])
def test_canonical_source_and_output_boundaries(tmp_path, escaped_name):
    root, output = tmp_path/"source", tmp_path/"output"
    source(root)
    output.mkdir()
    (output/"keep").write_text("unchanged")
    assert run(root, output).returncode == 1 and (output/"keep").read_text() == "unchanged"
    assert run(root, root/"nested").returncode == 1 and not (root/"nested").exists()
    assert run(root, tmp_path).returncode == 1
    alias = tmp_path/"alias"
    alias.symlink_to(root, target_is_directory=True)
    assert run(root, alias/"nested").returncode == 1
    victim = root/"Data"/escaped_name
    outside = tmp_path/"outside"
    victim.rename(outside)
    victim.symlink_to(outside)
    result = run(root, tmp_path/"escape-output")
    assert result.returncode == 1 and "escapes root" in result.stderr
    assert not (tmp_path/"escape-output").exists()
