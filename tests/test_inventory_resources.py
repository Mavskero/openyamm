"""Original scroll boundaries, bilingual item use, and ordered potion matrices."""
import csv
import json
import os
from pathlib import Path
import subprocess

import pytest

from test_story_resources import text, write_archive

TOOL = Path(os.environ.get("INVENTORY_IMPORT_TOOL", Path(__file__).parents[1]/"build-apple/inventory_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build inventory_import first")
POTIONS = [*range(222,291),806]
COMMENTS = {1520:'"Journal Entry, 23rd Apri',1521:'"Journal Entry, 30th June',
            1524:'"Journal Entry, 11th Dece',1526:'"Journal Entry, 2nd Janua',1527:'"Journal Entry, 19th Sept'}


def source(root, assets, chinese=True, multiline=True, mutate=None):
    data = root/"Data"
    (data/"Tables").mkdir(parents=True)
    tables = assets/"engine/data_tables"
    (tables/"english").mkdir(parents=True)
    scrolls = [["Item#","message text","Location","Item Name"],
               ["700",'First "quoted" paragraph\n\nLast paragraph',"Town","Recipe"],
               ["773","","","Empty Message Scroll"],
               ["1574","New hint","Lab","Philosopher stone hint"]]
    scrolls += [[str(i),"Entry body","Deyja",v[1:]] for i,v in COMMENTS.items()]
    items = []
    for id_ in sorted({*range(200,291),806,*[int(r[0]) for r in scrolls[1:]]}):
        equip = "Bottle" if id_<300 else "Weapon" if id_==806 else "Mscroll"
        items.append([str(id_),"icon",f"Item {id_}","1",equip,"Misc","0","0","0","0",
                      "Unknown","0","0","0","0","0","Description"])
    base = {"items.txt":items,"scroll.txt":scrolls}
    for name in ["potion.txt","potnotes.txt"]:
        matrix = [["","Name","Description","Effect"]+[""]*73]
        for id_ in [*range(200,291),806]:
            row = [str(id_),"Strange potion" if id_==806 else f"Potion {id_}","White Potion","Effect","1","0","0"]
            if id_==220: row += list(map(str,POTIONS))
            elif id_ in POTIONS: row += ["no" if p==id_ else "E1" if name=="potion.txt" else "0" for p in POTIONS]
            else:row += [""]*70
            matrix.append(row)
        for id_,target,value in [(222,223,"226"),(223,222,"225"),(282,223,"E4"),(262,223,"806")]:
            next(r for r in matrix if r[0]==str(id_))[7+POTIONS.index(target)] = value if name=="potion.txt" else "0"
        if name=="potnotes.txt":next(r for r in matrix if r[0]=="222")[8]="32"
        matrix += [["","Footer description","White Potion","Design comment"]+[""]*73]
        base[name]=matrix
    settings = [["Pot Id","Item Id","Required mastery","Drinkable","Usable","Notes"],
                ["0","220","N/A","N/A","N/A","Empty bottle"]]
    settings += [[str(i),str(i+220),str(i%5),"x" if i%2 else "-","-" if i%2 else "x",f"Potion {i}"]
                 for i in range(1,71)]
    settings[60][5] += " "  # Authored note padding is preserved; the production loader trims it.
    locale = [[r[0],"中文\x0e \x0e正\x07\x0f文\n\n第二段" if r[1] else "",*r[2:]] for r in scrolls[1:]]
    locale.insert(0,scrolls[0])
    next(r for r in locale if r[0]=="1574")[1:]=[""," 773","Empty Message Scroll"]
    localized_items = [r[:] for r in items]
    for row in localized_items: row[2]="中文物品" if chinese else row[2]
    notes = [["32","药水配方","Potion"]]
    if mutate:mutate(base,locale,settings,localized_items,notes)
    raw_scroll = text(base["scroll.txt"])
    if multiline:
        raw_scroll = text(base["scroll.txt"][:1])
        for row in base["scroll.txt"][1:]:
            raw_scroll += "\t".join(row)+"\n"
    # These exact truncated metadata quotes occur in both supplied archives.
    for id_,comment in COMMENTS.items():
        raw_scroll=raw_scroll.replace(comment[1:]+"\n",comment+"\n")
    raw = {"scroll.txt":raw_scroll}
    if multiline:
        for name in ["potion.txt","potnotes.txt"]:
            rows=[]
            for row in base[name]:
                if row[0] in ("282","283"):
                    rows.extend([row[:4],[""]+row[4:]])
                else:rows.append(row)
            raw[name]=text(rows)
    write_archive(data/"mmmerge.T.lod",base,raw_texts=raw)
    if chinese:
        write_archive(data/"10 LocZHCN.EnglishT.lod",{"scroll.txt":locale},True)
    (data/"Tables/Potion settings.txt").write_bytes(text(settings).encode("cp1252"))
    (tables/"items.txt").write_text(text(localized_items))
    (tables/"english/autonote.txt").write_text(text(notes))
    return base,locale,settings


def run(root,assets,out):
    return subprocess.run([str(TOOL),str(root),str(assets),str(out)],capture_output=True,text=True,timeout=15,umask=0o077)


def read(path):
    with path.open(newline="") as f:return list(csv.reader(f,delimiter="\t"))


@pytest.mark.parametrize("chinese,multiline",[(True,True),(False,True),(True,False)])
def test_reading_and_all_ordered_mixes_preserve_source(tmp_path,chinese,multiline):
    root,assets,out=[tmp_path/n for n in ["source","assets","out"]]
    base,locale,settings=source(root,assets,chinese,multiline)
    before={p:p.read_bytes() for folder in [root,assets] for p in folder.rglob("*") if p.is_file()}
    result=run(root,assets,out)
    assert result.returncode==0,result.stderr
    tables=out/"engine/data_tables"
    scrolls={int(r[0]):r for r in read(tables/"english/scroll.txt")}
    assert scrolls[700][1]==("中文正文\n\n第二段" if chinese else base["scroll.txt"][1][1])
    assert scrolls[1574][1:]==["New hint","Lab","Philosopher stone hint"]
    assert scrolls[773][1]==""
    for id_,comment in COMMENTS.items():assert scrolls[id_][3]==comment[1:]
    for name in ["potion.txt","potnotes.txt"]:
        rows=read(tables/"english"/name)
        expected={r[0]:r[:76] for r in base[name] if r[0].isdigit() and (r[0]=="220" or 222<=int(r[0])<=290)}
        assert {r[0]:r for r in rows}==expected and len(rows)==70
    assert read(tables/"potion_settings.txt")==settings
    report=json.loads((out/"inventory_conversion.json").read_text())
    assert report["readable_item_use_checks"]==7 and report["potion_combinations"]==4761
    assert report["potion_note_links"]==1 and report["asymmetric_note_cells"]==2
    assert report["untranslated_scroll_ids"]==([1574] if chinese else [])
    assert report["restored_glyphs"]==(6 if chinese else 0)
    assert report["omitted_matrix_operand_ids"]==[806]
    assert report["unsupported_result_rules"]==[[262,223,806]] and report["mix_runtime_checks"]==2
    assert report["matrix_continuation_lines"]==(4 if multiline else 0)
    assert not report["runtime_ready"] and not report["alchemy_gameplay_validated"]
    assert all(p.read_bytes()==b for p,b in before.items())
    assert all(p.stat().st_mode & 0o777==0o644 for p in out.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault",["scroll_duplicate","scroll_identity","scroll_item","scroll_quote","scroll_unfinished",
    "matrix_header","matrix_duplicate","matrix_missing","matrix_value","matrix_damage","matrix_result",
    "matrix_footer","potion_setting","potion_flag","potion_mastery","note_reference","note_category","stale_item"])
def test_invalid_identity_or_mechanics_leaves_no_output(tmp_path,fault):
    def mutate(b,l,s,items,notes):
        mix=b["potion.txt"]
        row=next(r for r in mix if r[0]=="222")
        if fault=="scroll_duplicate":b["scroll.txt"].append(b["scroll.txt"][1][:])
        if fault=="scroll_identity":l[1][3]="Different letter"
        if fault=="scroll_item":b["scroll.txt"][1][0]="65000"
        if fault=="scroll_quote":b["scroll.txt"][1][3]='"Broken'
        if fault=="scroll_unfinished":b["scroll.txt"][-1]=b["scroll.txt"][-1][:2]
        if fault=="matrix_header":next(r for r in mix if r[0]=="220")[7]="223"
        if fault=="matrix_duplicate":mix.append(row[:])
        if fault=="matrix_missing":mix.remove(row)
        if fault=="matrix_value":row[8]="225junk"
        if fault=="matrix_damage":row[8]="E0"
        if fault=="matrix_result":row[8]="806"
        if fault=="matrix_footer":mix[-1][7]="225"
        if fault=="potion_setting":s[-1][1]="806"
        if fault=="potion_flag":s[2][3]="yes"
        if fault=="potion_mastery":s[2][2]="5"
        if fault=="note_reference":notes[0][0]="99"
        if fault=="note_category":notes[0][2]="Misc"
        if fault=="stale_item":items[0][4]="Weapon"
    root,assets,out=[tmp_path/n for n in ["source","assets","out"]]
    source(root,assets,mutate=mutate)
    result=run(root,assets,out)
    assert result.returncode==1 and "Inventory conversion failed" in result.stderr
    assert not out.exists()


@pytest.mark.parametrize("victim",["source/Data/mmmerge.T.lod","source/Data/Tables/Potion settings.txt",
                                  "assets/engine/data_tables/items.txt"])
def test_source_and_asset_directory_boundaries(tmp_path,victim):
    root,assets,out=[tmp_path/n for n in ["source","assets","out"]]
    source(root,assets)
    for folder in [root,assets]:
        assert run(root,assets,folder/"nested").returncode==1
        assert not (folder/"nested").exists()
    out.mkdir();(out/"keep").write_text("unchanged")
    assert run(root,assets,out).returncode==1 and (out/"keep").read_text()=="unchanged"
    path=tmp_path/victim
    path.rename(tmp_path/"outside")
    path.symlink_to(tmp_path/"outside")
    result=run(root,assets,tmp_path/"new-output")
    assert result.returncode==1 and "escapes root" in result.stderr
    assert not (tmp_path/"new-output").exists()
