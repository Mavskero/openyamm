"""Exercise native combat table conversion and production spell-name lookup."""
import json
import os
from pathlib import Path
import subprocess

import pytest

import test_core_resources as core

TOOL = Path(os.environ.get("COMBAT_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/combat_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file() or not core.TOOL.is_file(), reason="Build combat/core import first")

UNUSED = [*range(104, 111), *range(115, 122), *range(126, 133)]
STATS_HEADER = ("Spell SpellPointsNormal SpellPointsExpert SpellPointsMaster SpellPointsGM DelayNormal "
                "DelayExpert DelayMaster DelayGM DamageAdd DamageDiceSides").split()


def enchant_tables():
    standard_names = ("Might|Intellect|Personality|Endurance|Accuracy|Speed|Luck|Hit Points|Spell Points|Armor Class|"
                      "Fire Resistance|Air Resistance|Water Resistance|Earth Resistance|Mind Resistance|Body Resistance|"
                      "Alchemy skill|Stealing skill|Disarm skill|ID Item skill|ID Monster skill|Armsmaster skill|"
                      "Dodge skill|Unarmed skill|Great might").split("|")
    special_suffixes = ("of Protection|of The Gods|of Carnage|of Cold|of Frost|of Ice|of Sparks|of Lightning|"
                        "of Thunderbolts|of Fire|of Flame|of Infernos|of Poison|of Venom|of Acid|Vampiric|of Recovery|"
                        "of Immunity|of Sanity|of Freedom|of Antidotes|of Alarms|of The Medusa|of Force|of Power|"
                        "of Air Magic|of Body Magic|of Dark Magic|of Earth Magic|of Fire Magic|of Light Magic|"
                        "of Mind Magic|of Spirit Magic|of Water Magic|of Thievery|of Shielding|of Regeneration|of Mana|"
                        "Ogre Slaying|Dragon Slaying|of Darkness|of Doom|of Earth|of Life|Rogues'|of The Dragon|"
                        "of The Eclipse|of The Golem|of The Moon|of The Phoenix|of The Sky|of The Stars|of The Sun|"
                        "of The Troll|of The Unicorn|Warriors'|Wizards'|Antique|Swift|Monks'|Thieves'|of Identifying|"
                        "Elemental Slaying|Undead Slaying|Of David|of Plenty|Assassins'|Barbarians'|of the Storm|"
                        "of the Ocean|of Water Walking|of Feather Falling|of Sparking").split("|")
    standard = [["Bonus Stat", "Of Name", "Arm", "Shld", "Helm", "Belt", "Cape", "Gaunt", "Boot", "Ring", "Amul"]]
    special = [["Bonus Stat", "Name Add", "W1", "W2", "Miss", "Arm", "Shld", "Helm", "Belt", "Cape", "Gaunt",
                "Boot", "Ring", "Amul", "Value", "Lvl", "Design notes"]]
    for index, name in enumerate(standard_names):
        standard.append([name, f"of {name}"] + (["0"] * 9 if index == 24 else ["5", "0", "10"] * 3))
    for index, suffix in enumerate(special_suffixes):
        special.append([f"Visible effect {index + 1}", suffix] + (["0"] * 12 if index == 72 else ["5"] * 12)
                       + ["X 2" if suffix == "Vampiric" else "1000", "B", "English design note"])
    # Real source tables have empty padding and treasure-level footer sections.
    standard[4][2] = "0 "  # Source numeric fields can have trailing whitespace.
    standard[13] += [""] * 5
    special[1] += [""] * 3
    standard += [["", ""], ["", "Bonus range for Standard by Level"], ["", "1", "0", "0"]]
    special += [["", ""], ["Treasure Level", "If treasure Level = 3 add all A and B"] + [""] * 15,
                ["A= 3 to 4", "If treasure Level = 4 add all A and B and C"]]
    return standard, special


def source(tmp_path, chinese=True, fault=None):
    tables = {}

    def add_combat(base):
        base["monsters.txt"][0][19] = "FireAr"
        base["monsters.txt"][1][19] = "Rock"
        base["monsters.txt"][2][23] = "Dagger"
        base["monsters.txt"][0][24:26] = ["30", "Fire Bolt,E,3"]
        base["monsters.txt"][1][24:26] = ["30", "Day-o-Gods,E,3"]
        if fault == "unknown_missile":
            base["monsters.txt"][0][19] = "FutureBeam"
        if fault == "unknown_spell":
            base["monsters.txt"][0][25] = "Future Storm,E,3"
        base["spells.txt"] = []
        names = {2: "Fire Bolt", 60: "Charm", 65: "Psychic Shockt", 83: "Day-o-Gods", 113: "Charm"}
        for id_ in range(1, 133):
            name = "Unused!!!" if id_ in UNUSED else names.get(id_, f"Spell {id_}")
            base["spells.txt"].append([str(id_), "1", name, "Fire", name, "Base text", "Normal", "Expert",
                                       "Master", "GM", "PMEC"])
        base["spells.txt"][-1] = base["spells.txt"][-1][:5]
        base["items.txt"].append(["1", "blade", "Sword"] + ["0"] * 14)
        for item in base["items.txt"]:
            item[3], item[11], item[10] = "73", "1", "Unknown item"
        base["rnditems.txt"] = [[r[0], "outdated_icon", "8", "4", "2", "1", "0", "0", r[2]]
                                for r in base["items.txt"]]
        if fault == "missing_weights":
            base["rnditems.txt"].pop()
        base["stditems.txt"], base["spcitems.txt"] = enchant_tables()
        if fault == "enchant_reordered":
            base["stditems.txt"][1], base["stditems.txt"][2] = base["stditems.txt"][2], base["stditems.txt"][1]
        if fault == "enchant_missing_slot":
            del base["spcitems.txt"][12]
        if fault == "enchant_bad_weight":
            base["stditems.txt"][4][2] = "10junk"
        if fault == "enchant_active_extension":
            base["spcitems.txt"][73][2] = "1"
        if fault == "enchant_unknown_extension":
            base["stditems.txt"][25][0] = "Future effect"
        if fault == "enchant_bad_price":
            base["spcitems.txt"][16][14] = "X 2 3"
        if fault == "enchant_populated_padding":
            base["stditems.txt"][13][11] = "unexpected metadata"
        tables.update(base)

    root = core.source(tmp_path / "source", chinese=False, table_mutator=add_combat)
    stats = [STATS_HEADER, [""] + ["0"] * 10]
    for row in tables["spells.txt"]:
        stats.append([row[2], "2", "3", "4", "5", "110", "100", "90", "80", "7", "3"])
    if fault == "spell_join":
        stats[3][0] = "Wrong spell"
    (root / "Data/Tables/Spells2.txt").write_text(core.text(stats))
    frames = [core.SFT_HEADER]
    for index, name in enumerate(["Null", "Human", "Bolt"]):
        frames.append([str(index), name, name, "1", "-", "x"] + ["-"] * 18)
    (root / "Data/Tables/SFT.txt").write_text(core.text(frames))
    objects = [core.OBJ_HEADER]
    for id_, name in [(0, "null"), (1, "blade"), (500, "Air"), (505, "Earth"), (510, "Fire"),
                      (515, "Water"), (520, "Body"), (525, "Mind"), (530, "Spirit"), (535, "Light"),
                      (540, "Dark"), (545, "Arrow"), (550, "FireArrow"), (555, "Laser"), (556, "explosion"),
                      (1010, "Fire Bolt"), (1011, "explosion")]:
        row = [name, str(id_), "16", "16"] + ["-"] * 11 + ["Bolt", "768", "4000", "0", "0", "0", "0"]
        for flag in (6, 8, 9, 10):
            row[flag] = "x"
        if fault == "wrong_projectile" and id_ == 505:
            row[0] = "Wrong object"
        objects.append(row)
    (root / "Data/Tables/ObjList.txt").write_text(core.text(objects))
    if chinese:
        localized = [r[:] if len(r) == 11 else r[:] + [""] * 6 for r in tables["spells.txt"]]
        for row in localized:
            row[2], row[5], row[10] = "火球术", '他说："魔法"\n第二行', "DIFFERENT"
        localized[1][2] = "Charm"  # Display label must not redirect semantic lookup to another spell.
        local_standard = [row[:] for row in tables["stditems.txt"]]
        local_special = [row[:] for row in tables["spcitems.txt"]]
        for row in local_standard[1:26]:
            row[0], row[1] = "力量", "力量类"
        for row in local_special[1:74]:
            row[0], row[1] = '增加抗性，说明含"引号"\n第二行', "保护类"
        local_special[1][1] = "of Fire"  # A display string must never redirect effect identity.
        if fault == "enchant_locale_mechanics":
            local_standard[1][2] = "99"
            local_special[1][14:16] = ["777", "C"]
        if fault == "enchant_locale_missing_slot":
            del local_special[12]
        core.archive(root / "Data/10 LocZHCN.EnglishT.lod", {
            "mapstats.txt": tables["mapstats.txt"], "placemon.txt": [["1", "居民"]],
            "spells.txt": localized, "stditems.txt": local_standard, "spcitems.txt": local_special}, True)
        locale = root / "Data/Text localization"
        locale.mkdir()
        (locale / "ZHCN_Monsters.txt").write_bytes(core.text([
            ["Table (of Game struct)", "Id", "Field", "New text"],
            ["MonstersTxt", "1", "Name", "居民"]]).encode("gb2312"))
        rows = [["Id", "Name", "NotIdentifiedName ", "Notes"],
                ["1", "长剑", "未知武器", "这是一把剑"], ["1004", "鲜血", "未知液体", "汉化说明"]]
        if fault == "duplicate_item_translation":
            rows.append(["1", "错误", "", ""])
        (locale / "ZHCN_ItemsTxt.txt").write_bytes(core.text(rows).encode("gb2312"))
    assets = tmp_path / "core"
    result = core.run(root, assets)
    assert result.returncode == 0, result.stderr
    return root, assets


def run(root, assets, output):
    return subprocess.run([str(TOOL), str(root), str(assets), str(output)], capture_output=True, text=True)


@pytest.mark.parametrize("chinese", [True, False])
def test_native_combat_preserves_mechanics_aliases_and_source(tmp_path, chinese):
    root, assets = source(tmp_path, chinese=chinese)
    before = {p: p.read_bytes() for parent in (root, assets) for p in parent.rglob("*") if p.is_file()}
    output = tmp_path / "combat"
    result = run(root, assets, output)
    assert result.returncode == 0, result.stderr
    table = output / "engine/data_tables"
    missiles = {r[0]: r for r in core.rows(table / "monster_projectiles.txt")}
    assert missiles["Rock"][1:] == ["505", "0"] and missiles["FireAr"][1:] == ["550", "0"]
    assert missiles["Ener"][1:] == ["555", "556"] and "Dagger" not in missiles
    spells = {int(r[0]): r for r in core.rows(table / "spells.txt")}
    assert len(spells) == 111 and all(i not in spells for i in UNUSED)
    assert spells[2][2] == ("Charm" if chinese else "Fire Bolt") and spells[2][24] == "Fire Bolt"
    assert spells[2][10] == "PMEC"  # Preserve spell permission/damage flags.
    assert spells[2][11:24] == ["2", "3", "4", "5", "110", "100", "90", "80", "10010", "1010", "1011", "7", "3"]
    assert spells[60][24] == spells[113][24] == "Charm"  # Preserve both original slots.
    items = {r[0]: r for r in core.rows(table / "items.txt")}
    assert items["1"][2] == ("长剑" if chinese else "Sword") and items["1"][3] == "73"
    assert items["1"][11] == "1"
    rnd = core.rows(table / "random_items.txt")
    assert all(r[1] == "outdated_icon" and r[2:8] == ["8", "4", "2", "1", "0", "0"] for r in rnd)
    manifest = json.loads((output / "combat_conversion.json").read_text())
    assert manifest["legacy_dagger_nonprojectile_monsters"] == [3]
    assert manifest["resolved_monster_spell_references"] == 2
    assert manifest["resolved_monster_missile_references"] == 2
    assert manifest["known_unsupported_monster_spells"] == [{"name": "day-o-gods", "monster_ids": [2]}]
    assert manifest["typed_validation"] and not manifest["runtime_ready"]
    assert manifest["preserved_spell_mechanical_cells"] == (111 if chinese else 0)
    standard = core.rows(table / "standard_item_enchants.txt")
    special = core.rows(table / "special_item_enchants.txt")
    assert len(standard) == 25 and len(special) == 73  # Footer text must not become extra enchant IDs.
    assert all(len(row) == 17 for row in standard) and all(len(row) == 21 for row in special)
    assert standard[0][0] == ("力量" if chinese else "Might") and standard[0][16] == "Might"
    assert special[0][1] == ("of Fire" if chinese else "of Protection") and special[0][20] == "of Protection"
    assert special[15][14] == "X 2" and special[15][20] == "Vampiric"
    assert special[0][16] == (special[0][0] if chinese else "English design note")
    assert standard[24][16] == "Great might" and special[72][20] == "of Sparking"
    assert manifest["unsupported_standard_enchant_ids"] == [25]
    assert manifest["unsupported_special_enchant_ids"] == [73]
    assert not manifest["unsupported_enchants_have_random_weight"]
    assert not manifest["enchant_treasure_ranges_imported"]
    assert manifest["translated_enchants"] == (98 if chinese else 0)
    assert all(p.read_bytes() == content for p, content in before.items())


@pytest.mark.parametrize("fault", ["enchant_reordered", "enchant_missing_slot", "enchant_bad_weight",
                                   "enchant_active_extension", "enchant_unknown_extension", "enchant_bad_price",
                                   "enchant_populated_padding", "enchant_locale_missing_slot"])
def test_rejects_unsafe_enchant_profiles(tmp_path, fault):
    root, assets = source(tmp_path, fault=fault)
    output = tmp_path / "combat"
    result = run(root, assets, output)
    assert result.returncode != 0 and "Enchantment conversion" in result.stderr
    assert not output.exists()


def test_old_enchant_translation_preserves_effective_source_values(tmp_path):
    root, assets = source(tmp_path, fault="enchant_locale_mechanics")
    output = tmp_path / "combat"
    result = run(root, assets, output)
    assert result.returncode == 0, result.stderr
    table = output / "engine/data_tables"
    assert core.rows(table / "standard_item_enchants.txt")[0][2] == "5"
    assert core.rows(table / "special_item_enchants.txt")[0][14:16] == ["1000", "B"]
    assert json.loads((output / "combat_conversion.json").read_text())["preserved_enchant_mechanical_cells"] == 3


@pytest.mark.parametrize("fault", ["spell_join", "unknown_missile", "unknown_spell", "wrong_projectile", "missing_weights",
                                   "duplicate_item_translation"])
def test_rejects_unsupported_or_mismatched_combat_data(tmp_path, fault):
    root, assets = source(tmp_path, fault=fault)
    output = tmp_path / "combat"
    result = run(root, assets, output)
    assert result.returncode != 0 and "Combat conversion failed" in result.stderr
    assert not output.exists()


def test_refuses_stale_core_mechanics(tmp_path):
    root, assets = source(tmp_path)
    path = assets / "engine/data_tables/monster_data.txt"
    rows = core.rows(path)
    rows[0][4] = "9999"
    path.write_text(core.text(rows))
    output = tmp_path / "combat"
    result = run(root, assets, output)
    assert result.returncode != 0 and "mechanics differ" in result.stderr
    assert not output.exists()


def test_refuses_overlapping_or_existing_output(tmp_path):
    root, assets = source(tmp_path)
    for output in (root / "converted", assets / "converted"):
        assert run(root, assets, output).returncode != 0 and not output.exists()
    output = tmp_path / "existing"
    output.mkdir()
    marker = output / "keep"
    marker.write_text("preserve")
    assert run(root, assets, output).returncode != 0 and marker.read_text() == "preserve"
