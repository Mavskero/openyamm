"""Native core conversion with synthetic data; no installed game or device needed."""
import csv
from io import StringIO
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("CORE_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/core_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build core_import first")

MON_HEADER = ("# Height Radius Velocity Radius2 TintB TintG TintR TintA SoundAttack SoundDie "
              "SoundGetHit SoundFidget Name FramesStand FramesWalk FramesAttack FramesShoot "
              "FramesStun FramesDie FramesDead FramesFidget").split()
SFT_HEADER = ("# GroupName SpriteName Scale Luminous GroupStart Image1 Center Fidget Loaded "
              "Mirror0 Mirror1 Mirror2 Mirror3 Mirror4 Mirror5 Mirror6 Mirror7 Images3 Glow "
              "Transparent LightRadius PaletteId Time").split()
OBJ_HEADER = ("Name Id Radius Height Invisible Untouchable Temporary LifetimeInSFT NoPickup "
              "NoGravity InterceptAction Bounce TrailParticles TrailFire TrailLine SFTGroup "
              "LifeTime Speed ParticleR ParticleG ParticleB ParticleA").split()

# An independently authored small table in the supported legacy handler format.
DROP_SCRIPT = '''local random, floor, ceil = math.random, math.floor, math.ceil
local MonsterItems = {
[1] = 1004
}
function events.MonsterKilled(mon)
    if mon.Ally == 9999 then -- no drop from reanimated monsters
        return
    end
    local monKind = ceil(mon.Id/3)
    local Mul = mon.Id - monKind*3 + 3
    local ItemId = MonsterItems[monKind]
    if ItemId and random(100) + Mul*5 > 80 then
        local t = {MonsterIndex = mon:GetIndex(), Monster = mon, ItemId = ItemId, Handled = false}
        events.Call("MonsterDropItem", t)
        if not t.Handled then
            evt.SummonObject(ItemId, mon.X, mon.Y, mon.Z + 100, 100)
        end
    end
end
'''


def text(rows):
    out = StringIO()
    csv.writer(out, delimiter="\t", lineterminator="\n").writerows(rows)
    return out.getvalue()


def archive(path, tables, chinese=False):
    lod(path, {name: wrapped(name, text(rows).encode("gb2312" if chinese else "cp1252"))
               for name, rows in tables.items()})


def source(root, fault=None, chinese=True, table_mutator=None):
    data = root / "Data"
    (data / "Tables").mkdir(parents=True)
    maps = []
    for id_, name in [(151, "oute3.odm"), (193, "zddb05.blv"), (194, "zddb06.blv")]:
        row = [str(id_), 'Town "name"', name] + ["0"] * 31
        row[33] = "-"
        maps.append(row)
    monsters, monlist = [], [MON_HEADER]
    for index in range(3):
        picture = f"Resident {index + 1}"
        monsters.append([str(index + 1), f"Name {index + 1}", picture, "4", "13", "2", "5"]
                        + ["0"] * 32)
        monlist.append([str(index), "160", "60", "200", "0", "999", "255", "126", "0"]
                       + ["1000", "1001", "1002", "1003", picture] + ["Human"] * 8)
    if fault == "monster_join":
        monlist[1][13] = "Wrong appearance"
    if fault == "descriptor_gap":
        monlist[2][0] = "99"
    english_global = {
        56: "Day", 57: "Days", 68: "Donate", 86: "Fill Packs", 97: "Gold", 104: "Heal",
        113: "Identify", 128: "Travel", 134: "Buy Standard", 140: "Your packs are already full!",
        152: "Buy Special", 155: "You don't have enough gold", 159: "Display Inventory",
        160: "Learn Skills", 168: "Points", 178: "Rent Room", 179: "Repair",
        195: "Select the Item to Buy", 196: "Select the Special Item to Buy",
        197: "Select the Item to Identify", 198: "Select the Item to Repair",
        199: "Select the Item to Sell", 200: "Sell", 404: "Travel days", 405: "Travel cost",
        414: "Open hours", 446: "Identify Item Failed", 448: "Repair Item Failed",
        527: "Thank You!", 529: "Sorry, but we are unable to train you.",
        536: "With your skills, you should be working here as a teacher",
        537: "Train to level", 538: "You need more experience", 540: "Sell Items",
        541: "Identify Items", 542: "Repair Items", 561: "Sorry, come back another day",
        563: "Pack is Full", 569: "Done!", 570: "Good as New!", 663: "One day travel",
        164: "%s hits %s for %lu damage", 189: "%s shoots %s for %lu points",
        175: "%s inflicts %lu points killing %s", 635: "%s stuns %s", 636: "%s paralyzes %s", 480: "Enemies are nearby.",
    }
    tables = {
        "global.txt": [[str(id_), value] for id_, value in sorted(english_global.items())],
        "mapstats.txt": maps,
        "monsters.txt": monsters,
        "placemon.txt": [["1", "Unique"]],
        "hostile.txt": [["", "Party", "Resident"], ["Party", "0", "1"], ["Resident", "2", "0"]],
        "items.txt": [["1004", "bottle", "Blood"] + ["0"] * 14],
    }
    if fault == "duplicate_map":
        maps.append(list(maps[0]))
    if fault == "relations":
        tables["hostile.txt"][2][0] = "Wrong family"
    if table_mutator:
        table_mutator(tables)
    archive(data / "mmmerge.T.lod", tables)
    lod(data / "mm6.games.lod", {"oute3.odm": b"owned map", "zddb06.blv": b"owned dungeon"})
    lod(data / "patch.games.lod", {"zddb06.blv": b"updated dungeon"})
    if fault == "ambiguous_world":
        lod(data / "mm7.games.lod", {"oute3.odm": b"ambiguous map"})
    if chinese:
        chinese_global = {
            **english_global,
            480: "附近有敌人。",
            56: "日", 57: "天", 68: "捐赠", 86: "购买食物", 97: "金币", 104: "治疗",
            113: "鉴定", 128: "旅行", 134: "购买标准物品", 140: "你的行囊已经装满了！",
            152: "购买特殊物品", 155: "你的金币不够", 159: "显示行囊", 160: "学习技能",
            168: "技能点", 178: "租房间", 179: "修理", 195: "选择要购买的物品",
            196: "选择要购买的特殊物品", 197: "选择要鉴定的物品", 198: "选择要修理的物品",
            199: "选择要出售的物品", 200: "出售", 404: "旅行天数", 405: "旅费", 414: "开放时间",
            446: "鉴定物品失败", 448: "修理物品失败", 527: "谢谢你！", 529: "抱歉，我们不能训练你",
            536: "依你的水平，都可以在此当老师了", 537: "训练至等级", 538: "你需要更多经验",
            540: "出售物品", 541: "鉴定物品", 542: "修理物品", 561: "抱歉，请改天再来",
            563: "行囊已满", 569: "完成", 570: "完好如新", 663: "一天旅行",
            164: "%s攻击%s，造成%lu点破坏", 189: "%s射击%s，造成%lu点伤害",
            175: "%s造成%lu点伤害，杀死%s", 635: "%s打昏了%s", 636: "%s麻痹了%s",
        }
        overlays = [list(row) for row in maps]
        for row in overlays:
            row[1], row[6] = "城镇", "999"  # Translation must not change respawn mechanics.
        overlays[0][2] = "wrong.odm"  # A stale ID/filename pairing must not localize another map.
        archive(data / "10 LocZHCN.EnglishT.lod", {
            "global.txt": [[str(id_), value] for id_, value in sorted(chinese_global.items())],
            "mapstats.txt": overlays, "placemon.txt": [["1", "村民"]]}, True)
        locale = data / "Text localization"
        locale.mkdir()
        (locale / "ZHCN_Monsters.txt").write_bytes(text([
            ["Table (of Game struct)", "Id", "Field", "New text"],
            ["MonstersTxt", "0", "Name", "未使用"],
            ["MonstersTxt", "1", "Name", "居民"],
            ["MonstersTxt", "2", "Name", "村长"],
        ]).encode("gb2312"))
    bolster = [[str(id_), "Pending", "3"] + ["0"] * 7 for id_ in (151, 193, 194)]
    continents = [["3", "Enroth"] + ["0"] * 21]
    frames = [SFT_HEADER]
    for index, group in enumerate(["Null", "Human", "Bottle"]):
        frames.append([str(index), group, group + "1", "1", "-", "x"] + ["-"] * 18)
    objects = [OBJ_HEADER]
    for id_, group in [(0, "Null"), (550, "Bottle"), (1, "Human")]:
        objects.append([group, str(id_), "32", "64"] + ["-"] * 11
                       + [group, "10", "20", "1", "2", "3", "4"])
    objects[2][11] = "x"  # Bounce bit 7; order differs from object IDs.
    if fault == "missing_sprite":
        objects[2][15] = "Absent"
    if fault == "object_range":
        objects[2][2] = "99999"
    for name, rows in [("MonList.txt", monlist), ("SFT.txt", frames), ("ObjList.txt", objects),
                       ("Bolster - maps.txt", bolster), ("Continent settings.txt", continents)]:
        (data / "Tables" / name).write_text(text(rows))
    scripts = root / "Scripts/General"
    scripts.mkdir(parents=True)
    script = DROP_SCRIPT.replace("> 80", "> 70") if fault == "drop_formula" else DROP_SCRIPT
    (scripts / "MonsterItems.lua").write_text(script)
    return root


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True)


def rows(path):
    with path.open() as stream:
        return list(csv.reader(stream, delimiter="\t"))


@pytest.mark.parametrize("chinese", [True, False])
def test_complete_typed_conversion_preserves_ids_mechanics_and_source(tmp_path, chinese):
    root = source(tmp_path / "source", chinese=chinese)
    before = {p.relative_to(root): p.read_bytes() for p in root.rglob("*") if p.is_file()}
    output = tmp_path / "core"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    tables = output / "engine/data_tables"
    maps = {r[0]: r for r in rows(tables / "map_stats.txt")}
    assert maps["151"][1] == 'Town "name"'  # Mismatched translation ID/file rejected.
    assert maps["194"][1] == ("城镇" if chinese else 'Town "name"')
    assert all(r[34] == "mm6" and r[6] == "0" for r in maps.values())
    monsters = rows(tables / "monster_data.txt")
    assert monsters[0][:5] == ["1", "居民" if chinese else "Name 1", "Resident 1", "4", "13"]
    assert monsters[2][1] == "Name 3"  # No shifting of the language pack's zero slot.
    desc = rows(tables / "monster_descriptors.txt")
    assert [r[0] for r in desc] == ["1", "2", "3"]
    assert int(desc[0][6]) == (999 & 255) | 255 << 8 | 126 << 16
    objects = rows(tables / "object_list.txt")
    assert [r[2] for r in objects] == ["0", "550", "1"]  # Preserve description index.
    assert objects[1][7] == "2" and objects[1][12] == "128"
    assert int(objects[1][8]) == 0x04010203
    drops = rows(tables / "monster_death_drops.txt")
    assert [r[4] for r in drops] == ["25", "30", "35"] and all(r[2] == "1004" for r in drops)
    report = json.loads((output / "core_conversion.json").read_text())
    assert report["typed_validation"] and not report["runtime_ready"]
    assert report["missing_map_geometry"] == [{"id": 193, "file": "zddb05.blv", "world": "mm6"}]
    assert report["map_translation_identity_mismatches"] == (1 if chinese else 0)
    assert report["unused_monster_translation_count"] == (1 if chinese else 0)
    assert report["combat_text_count"] == 6
    assert report["combat_text_source"] == (
        "Data/10 LocZHCN.EnglishT.lod::global.txt" if chinese else "Data/mmmerge.T.lod::global.txt")
    assert report["house_service_text_count"] == 72
    assert report["house_service_source_ids"] == [
        56, 57, 68, 86, 97, 104, 113, 128, 134, 140, 152, 155, 159, 160, 168, 178, 179,
        195, 196, 197, 198, 199, 200, 404, 405, 414, 446, 448, 527, 529, 536, 537, 538,
        540, 541, 542, 561, 563, 569, 570, 663]
    assert report["house_service_text_source"] == (
        "Data/10 LocZHCN.EnglishT.lod::global.txt" if chinese else "Data/mmmerge.T.lod::global.txt")
    house_service = dict(rows(tables / "house_service_text.txt"))
    assert house_service["temple.heal_action"] == ("治疗（{price} 金币）" if chinese else "Heal {price} Gold")
    assert house_service["temple.learn_skills_action"] == ("学习技能" if chinese else "Learn Skills")
    assert house_service["temple.donate_success"] == ("谢谢你！" if chinese else "Thank You!")
    assert house_service["temple.learn_insufficient_gold"] == (
        "你的金币不够" if chinese else "You don't have enough gold")
    assert house_service["shop.display_equipment_action"] == (
        "显示行囊" if chinese else "Display Inventory")
    assert house_service["shop.identify_failed"] == (
        "鉴定物品失败" if chinese else "Identify Item Failed")
    assert house_service["transport.no_route_today"] == (
        "抱歉，请改天再来" if chinese else "Sorry, come back another day")
    assert rows(tables / "combat_text.txt") == (
        [["combat.melee_hit", "%s攻击%s，造成%lu点破坏"],
         ["combat.projectile_hit", "%s射击%s，造成%lu点伤害"],
         ["combat.kill", "%s造成%lu点伤害，杀死%s"],
         ["combat.stun", "%s打昏了%s"], ["combat.paralyze", "%s麻痹了%s"],
         ["combat.enemy_nearby", "附近有敌人。"]] if chinese else
        [["combat.melee_hit", "%s hits %s for %lu damage"],
         ["combat.projectile_hit", "%s shoots %s for %lu points"],
         ["combat.kill", "%s inflicts %lu points killing %s"],
         ["combat.stun", "%s stuns %s"], ["combat.paralyze", "%s paralyzes %s"],
         ["combat.enemy_nearby", "Enemies are nearby."]])
    assert before == {p.relative_to(root): p.read_bytes() for p in root.rglob("*") if p.is_file()}


@pytest.mark.parametrize("fault", ["monster_join", "descriptor_gap", "duplicate_map", "ambiguous_world",
                                   "relations", "missing_sprite", "object_range", "drop_formula"])
def test_rejects_incompatible_or_broken_tables_without_partial_output(tmp_path, fault):
    root = source(tmp_path / "source", fault=fault)
    output = tmp_path / "core"
    result = run(root, output)
    assert result.returncode != 0 and "Core conversion failed" in result.stderr
    assert not output.exists()


@pytest.mark.parametrize("fault", ["missing", "duplicate", "unsafe_argument", "reordered_argument"])
def test_rejects_invalid_combat_source_before_writing_output(tmp_path, fault):
    def mutate(tables):
        records = tables["global.txt"]
        by_id = {row[0]: row for row in records}
        if fault == "missing":
            records.remove(by_id["636"])
        elif fault == "duplicate":
            records.append(list(by_id["164"]))
        elif fault == "unsafe_argument":
            by_id["164"][1] = "%s hits %s for %n"
        else:
            by_id["175"][1] = "%s kills %s for %lu"
    root = source(tmp_path / "source", chinese=False, table_mutator=mutate)
    output = tmp_path / "core"
    result = run(root, output)
    assert result.returncode != 0 and "global.txt" in result.stderr
    assert not output.exists()


def test_incomplete_chinese_catalog_does_not_fall_back_to_english(tmp_path):
    root = source(tmp_path / "source")
    archive(root / "Data/10 LocZHCN.EnglishT.lod", {"global.txt": [["164", "%s攻击%s，造成%lu点破坏"]]}, True)
    output = tmp_path / "core"
    result = run(root, output)
    assert result.returncode != 0 and "Missing combat text in global.txt" in result.stderr
    assert not output.exists()


def test_protects_source_and_existing_outputs(tmp_path):
    root = source(tmp_path / "source")
    result = run(root, root / "converted")
    assert result.returncode != 0 and "outside" in result.stderr
    existing = tmp_path / "existing"
    existing.mkdir()
    marker = existing / "keep.txt"
    marker.write_text("User file")
    assert run(root, existing).returncode != 0 and marker.read_text() == "User file"


def test_conflicting_old_translation_keeps_base_name_and_reports_id(tmp_path):
    root = source(tmp_path / "source")
    locale = root / "Data/Text localization/ZHCN_Monsters.txt"
    with locale.open("ab") as stream:
        stream.write(text([["MonstersTxt", "2", "Name", "反应器"]]).encode("gb2312"))
    output = tmp_path / "core"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    assert rows(output / "engine/data_tables/monster_data.txt")[1][1] == "Name 2"
    report = json.loads((output / "core_conversion.json").read_text())
    assert report["conflicting_monster_translation_ids"] == [2]
    assert report["monster_name_translations"] == 1


def test_blank_sft_reference_keeps_last_legacy_index_without_inventing_art(tmp_path):
    root = source(tmp_path / "source")
    sft = root / "Data/Tables/SFT.txt"
    frames = rows(sft)
    for index in (3, 4):
        frames.append([str(index), "", "unused", "1", "-", "x"] + ["-"] * 18)
    sft.write_text(text(frames))
    path = root / "Data/Tables/ObjList.txt"
    objects = rows(path)
    objects[2][15] = ""
    path.write_text(text(objects))
    output = tmp_path / "core"
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    exported = rows(output / "engine/data_tables/object_list.txt")
    assert exported[1][1] == "" and exported[1][7] == "4"
    report = json.loads((output / "core_conversion.json").read_text())
    assert report["unnamed_sft_group_slots"] == 2 and report["unnamed_object_sprite_ids"] == [550]


def test_loose_table_cannot_escape_source_through_symlink(tmp_path):
    root = source(tmp_path / "source")
    table = root / "Data/Tables/MonList.txt"
    outside = tmp_path / "outside.txt"
    table.rename(outside)
    table.symlink_to(outside)
    output = tmp_path / "core"
    result = run(root, output)
    assert result.returncode != 0 and "escapes import root" in result.stderr
    assert not output.exists()
