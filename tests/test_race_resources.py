"""Race mechanics and creation restrictions through the native production tables."""
import csv
import json
import os
from pathlib import Path
import subprocess

import pytest
import yaml

from test_class_resources import CLASSES, SKILLS, TOOL as CLASS_TOOL, source as class_source
from test_story_resources import text

TOOL = Path(os.environ.get("RACE_IMPORT_TOOL", Path(__file__).parents[1]/"build-apple/race_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file() or not CLASS_TOOL.is_file(), reason="Build race_import and class_import")
RACES = "Human Vampire DarkElf Minotaur Troll Dragon Undead Elf Goblin Dwarf Zombie".split()
FAMILIES = ("Archer,Cleric,Dark Elf,Dragon,Druid,Knight,Minotaur,Monk,Paladin,Ranger,Thief,Troll,Vampire,"
            "Sorcerer,Necromancer,Peasant").split(",")
STATS = "Might Intellect Personality Endurance Accuracy Speed Luck".split()
CONTINENTS = ["Jadam", "Antagarich", "Enroth", "Between time"]


def read(path):
    with path.open(newline="") as stream:
        return list(csv.reader(stream, delimiter="\t"))


def source(root, class_assets, mutate=None):
    class_source(root)
    result = subprocess.run([str(CLASS_TOOL), str(root), str(class_assets)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    directory = root/"Data/Tables"
    race_headers = [f"{('Dark elf' if r==2 else race)} ({r})" for r,race in enumerate(RACES)]
    stats = [["Stat"]+race_headers+["n/u"]*7]
    for sk, name in enumerate(STATS):
        stats.append([name]+[f"{11+r%4}/{25+r}" for r in range(11)]+["11/25"]*7)
        stats.append(["+ add"]+["2" if (r+sk)%2==0 else "1/2" for r in range(11)]+["1"]*7)
    targets = [f"{name}({r})" for r,name in enumerate(RACES)]
    targets += [f"{race} - {family}" for family in FAMILIES for race in RACES]
    rules = [[""]+targets]+[[sk]+["-"]*187 for sk in SKILLS]
    # Global rules cover add-only, excluded family, each mana condition, and grants.
    for index, exception in enumerate(["0", "2", "I", "P", "IP", "S", "W"]):
        rules[index+1][index+1] = f"E/1/{exception}"
    rules[39][1] = "0/1/0"
    rules[1][17] = "G/0/W"  # Dragon/Archer class pool ignores the exception, as authored.
    rules[2][5] = ""  # A truly blank cell remains no rule.
    selection = [["Race/Class"]+CLASSES]
    for race in range(11):
        selection.append([str(race)]+["x" if race not in [6,10] and i in [0,4,48] else "-" for i in range(53)])
    selection.append(["", "Continents:"])
    for name in CONTINENTS:
        selection += [[name,"Portraits exceptions:","portrait201"],
                      ["","Available classes:","0","4","48"], ["","Available races:","0","7"]]
    continents = [["#","Note"]]+[[str(i+1),n] for i,n in enumerate(CONTINENTS)]
    tables = {"Class Starting Stats.txt":stats, "Race Skills.txt":rules,
              "Character selection.txt":selection, "Continent settings.txt":continents}
    if mutate:
        mutate(tables)
    for name, rows in tables.items():
        (directory/name).write_bytes(text(rows).encode("cp1252"))
    return tables


def run(root, class_assets, output):
    return subprocess.run([str(TOOL), str(root), str(class_assets), str(output)], capture_output=True,
                          text=True, timeout=15, umask=0o077)


def test_race_stats_rules_and_selection_keep_source_values(tmp_path):
    root, class_assets, output = [tmp_path/n for n in ["source","classes","output"]]
    tables = source(root, class_assets)
    before = {p:p.read_bytes() for folder in [root,class_assets] for p in folder.rglob("*") if p.is_file()}
    result = run(root,class_assets,output)
    assert result.returncode == 0, result.stderr
    directory = output/"engine/data_tables"
    stats = read(directory/"class_starting_stats.txt")
    assert len(stats)==15 and all(len(r)==12 for r in stats)
    for i,row in enumerate(stats):
        original = tables["Class Starting Stats.txt"][i][:12]
        if i>0 and i%2==0:
            original = [original[0]]+[v if "/" in v else v+"/1" for v in original[1:]]
        assert row==original
    assert stats[2][1]=="2/1"  # Base 11 with source '2' must NOT infer dropped cost 2.
    rules = yaml.safe_load((directory/"race_skills.yml").read_text())["rules"]
    assert len(rules)==9
    assert next(r for r in rules if r["raw_token"]=="0/1/0")["skill"]=="Learning"
    assert str(next(r for r in rules if r.get("class_kind"))["class_kind"])=="1"
    selection = yaml.safe_load((directory/"character_selection.yml").read_text())
    assert len(selection["race_class_availability"])==11
    assert selection["race_class_availability"][0]["classes"]==["Archer","Cleric","Peasant"]
    assert selection["race_class_availability"][6]["classes"]==[]
    assert selection["race_class_availability"][10]["classes"]==[]
    assert [c["key"] for c in selection["new_game_continents"]]==["jadame","antagarich","enroth","between_time"]
    for index,c in enumerate(selection["new_game_continents"]):
        assert c["id"]==index+1 and c["available_class_ids"]==[0,4,48]
        assert c["available_race_ids"]==[0,7] and c["portrait_exceptions"]==["portrait201"]
    report = json.loads((output/"race_conversion.json").read_text())
    assert report["race_class_skill_checks"]==51*11*39
    assert report["inactive_race_ids"]==list(range(11,18))
    assert report["stat_entries"]==77 and report["selected_race_class_pairs"]==27
    assert report["numeric_zero_rules_requiring_legacy_parser_review"]==1
    assert report["blank_skill_cells"]==1 and report["typed_validation"]
    assert not report["runtime_ready"] and not report["legacy_lua_parity"] and not report["character_creation_ready"]
    assert all(p.read_bytes()==b for p,b in before.items())
    assert all(p.stat().st_mode & 0o777==0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["stat_id","stat_order","bad_range","zero_cost","negative","bad_pair",
    "race_skill_order","race_target","rule_parts","rule_mastery","rule_add","rule_exception",
    "selection_marker","inactive_class","duplicate_class","unknown_race","continent_identity"])
def test_malformed_sources_are_rejected(tmp_path,fault):
    def mutate(t):
        stats,rules,selection,continents = [t[n] for n in ["Class Starting Stats.txt","Race Skills.txt",
            "Character selection.txt","Continent settings.txt"]]
        if fault=="stat_id": stats[0][1]="Human (9)"
        if fault=="stat_order": stats[1][0]="Intellect"
        if fault=="bad_range": stats[1][1]="30/20"
        if fault=="zero_cost": stats[2][1]="1/0"
        if fault=="negative": stats[1][1]="-1/25"
        if fault=="bad_pair": stats[2][1]="1/2/3"
        if fault=="race_skill_order": rules[1][0]="Sword"
        if fault=="race_target": rules[0][17]="Dragon - Cleric"
        if fault=="rule_parts": rules[1][1]="E/1/0/1"
        if fault=="rule_mastery": rules[1][1]="Z/1/0"
        if fault=="rule_add": rules[1][1]="E/99/0"
        if fault=="rule_exception": rules[1][1]="E/1/unknown"
        if fault=="selection_marker": selection[1][1]="yes"
        if fault=="inactive_class": selection[1][50]="x"
        if fault=="duplicate_class": selection[14].append("48")
        if fault=="unknown_race": selection[15][2]="11"
        if fault=="continent_identity": continents[1][1]="Enroth"
    root,class_assets,output = [tmp_path/n for n in ["source","classes","output"]]
    source(root,class_assets,mutate)
    result=run(root,class_assets,output)
    assert result.returncode==1 and "Race conversion failed" in result.stderr
    assert not output.exists()


def test_stale_class_inputs_are_rejected(tmp_path):
    root,class_assets,output = [tmp_path/n for n in ["source","classes","output"]]
    source(root,class_assets)
    path=class_assets/"engine/data_tables/class_multipliers.txt"
    rows=read(path); rows[1][1]="99"
    path.write_text(text(rows))
    result=run(root,class_assets,output)
    assert result.returncode==1 and "Stale class HP/SP" in result.stderr and not output.exists()


def test_input_and_output_boundaries(tmp_path):
    root,class_assets,output = [tmp_path/n for n in ["source","classes","output"]]
    source(root,class_assets)
    output.mkdir(); (output/"keep").write_text("unchanged")
    assert run(root,class_assets,output).returncode==1 and (output/"keep").read_text()=="unchanged"
    for folder in [root,class_assets]:
        assert run(root,class_assets,folder/"nested").returncode==1 and not (folder/"nested").exists()
    assert run(root,class_assets,tmp_path).returncode==1
    alias=tmp_path/"alias"; alias.symlink_to(class_assets,target_is_directory=True)
    assert run(root,class_assets,alias/"nested").returncode==1
    victim=root/"Data/Tables/Race Skills.txt"; outside=tmp_path/"outside"
    victim.rename(outside); victim.symlink_to(outside)
    result=run(root,class_assets,tmp_path/"escape-output")
    assert result.returncode==1 and "escapes root" in result.stderr
    assert not (tmp_path/"escape-output").exists()
