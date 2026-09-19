"""Shared frame timing, original group boundaries and available icon decoding."""
import csv
import json
import os
from pathlib import Path
import subprocess

from PIL import Image
import pytest

from test_menu_resources import lod, wrapped
from test_story_resources import text

TOOL = Path(os.environ.get("FRAME_IMPORT_TOOL",Path(__file__).parents[1]/"build-apple/frame_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(),reason="Build frame_import first")


def source(root, fault=None):
    data=root/"Data"
    (data/"Tables").mkdir(parents=True)
    portraits=[["GroupId","FrameIndex","Time","GroupStart"],
               ["0","1","8","x"],["1","1","8","x"],["13","2","2","x"],["0","3","4","-"],
               ["58","5","8","x"],["0","19","8","-"],["0","18","8","-"]]
    icons=[["#","GroupName","IconName","Time","GroupStart"],
           ["0","","pending","0","x"],["1","Glow01","Glow01A","2","x"],
           ["2","","glow01b","1","-"],["3","SPtext01A","SPtext01A","1","x"],
           ["4","SPtext01B","","1","-"]]
    if fault=="portrait_duplicate":portraits[3][0]="1"
    if fault=="portrait_continuation":portraits[4][0]="14"
    if fault=="portrait_zero_time":portraits[2][2]="0"
    if fault=="portrait_zero_texture":portraits[2][1]="0"
    if fault=="portrait_time_overflow":portraits[3][2]="65535"
    if fault=="first_not_start":portraits[1][3]="-"
    if fault=="marker":icons[2][-1]="yes"
    if fault=="icon_order":icons[3][0]="3"
    if fault=="icon_duplicate":icons[4][1]="glow01"
    if fault=="icon_zero_time":icons[3][3]="0"
    if fault=="icon_name":icons[2][2]="../escape"
    if fault=="icon_number":icons[2][3]="1x"
    if fault=="header":icons[0][3]="Unknown"
    if fault=="padding":icons[3].append("extra")
    for name,rows in [('PFT.txt',portraits),('IFT.txt',icons)]:
        (data/"Tables"/name).write_bytes(text(rows).encode("cp1252"))
    palette=bytearray(768)
    palette[3:6]=bytes((250,20,30));palette[6:9]=bytes((10,240,40))
    image=wrapped("glow01a",bytes([1,2,1,2,1,2]),3,2,palette,name_size=16)
    lod(data/"mm7.icons.lod",{"GLOW01A":image},version="MMVI")
    # A same-named sprite member must not compete for icon ownership.
    lod(data/"mm6.sprites.lod",{"glow01a":b"not an icon"})
    if fault=="ambiguous":lod(data/"mmmerge.D.lod",{"glow01a":image},version="MMVI")
    if fault=="damaged_image":lod(data/"mm7.icons.lod",{"glow01a":b"bad bitmap"})
    return portraits,icons


def run(root,out):
    return subprocess.run([str(TOOL),str(root),str(out)],capture_output=True,text=True,timeout=15,umask=0o077)


@pytest.mark.parametrize("base_first", [True, False])
def test_original_mm8_icons_are_below_known_merge_frames(tmp_path, base_first):
    root, out = tmp_path / "source", tmp_path / "out"
    (root / "Data").mkdir(parents=True)
    palette = bytearray(768)
    palette[3:6] = bytes((80, 80, 80))
    base = wrapped("glow01a", bytes([1] * 6), 3, 2, palette, name_size=16)
    if base_first:
        lod(root / "Data/icons.lod", {"glow01a": base}, version="MMVI")
    source(root)
    if not base_first:
        lod(root / "Data/icons.lod", {"glow01a": base}, version="MMVI")
    result = run(root, out)
    assert result.returncode == 0, result.stderr
    image = Image.open(out / "engine/icons/glow01a.bmp").convert("RGB")
    assert image.getpixel((0, 0)) == (250, 20, 30)
    report = json.loads((out / "frame_conversion.json").read_text())
    assert report["images"] == [{"name": "glow01a.bmp", "archive": "mm7.icons.lod", "width": 3, "height": 2}]


def test_unknown_frame_layer_cannot_override_original_icons(tmp_path):
    root, out = tmp_path / "source", tmp_path / "out"
    source(root)
    known = root / "Data/mm7.icons.lod"
    original = known.read_bytes()
    known.unlink()
    (root / "Data/icons.lod").write_bytes(original)
    (root / "Data/custom.icons.lod").write_bytes(original)
    result = run(root, out)
    assert result.returncode != 0
    assert "Ambiguous frame image ownership" in result.stderr
    assert not out.exists()


def read(path):
    with path.open(newline="") as stream:return list(csv.reader(stream,delimiter="\t"))


def test_original_frame_ids_flags_times_and_partial_art(tmp_path):
    root,out=tmp_path/"source",tmp_path/"out"
    source(root)
    before={p:p.read_bytes() for p in root.rglob("*") if p.is_file()}
    result=run(root,out)
    assert result.returncode==0,result.stderr
    tables=out/"engine/data_tables"
    assert read(tables/"portrait_frame_data.txt")==[
        ["0","1","8","8","4"],["1","1","8","8","4"],
        ["13","2","2","6","5"],["0","3","4","0","0"],
        ["58","5","8","24","5"],["0","19","8","0","1"],["0","18","8","0","0"]]
    assert read(tables/"icon_frame_data.txt")==[
        ["","pending","0","0","4"],["glow01","glow01a","2","3","5"],
        ["","glow01b","1","0","0"],["sptext01a","sptext01a","1","2","5"],
        ["sptext01b","","1","0","0"]]
    indexed=Image.open(out/"engine/icons/glow01a.bmp")
    assert indexed.mode=="P" and indexed.tobytes()==bytes([1,2,1,2,1,2])
    image=indexed.convert("RGB")
    assert image.size==(3,2)
    assert [image.getpixel((x,y)) for y in range(2) for x in range(3)]==[(250,20,30),(10,240,40)]*3
    report=json.loads((out/"frame_conversion.json").read_text())
    assert report["portrait_frames"]==7 and report["portrait_groups_including_zero"]==4
    assert report["icon_frames"]==5 and report["icon_groups_including_placeholder"]==3
    assert report["frame_selection_checks"]==35
    assert report["missing_icon_images"]==["glow01b","pending","sptext01a"]
    assert report["blank_texture_frame_ids"]==[4]
    assert report["images"]==[{"name":"glow01a.bmp","archive":"mm7.icons.lod","width":3,"height":2}]
    assert report["typed_validation"] and not report["runtime_ready"] and not report["character_portrait_art_ready"]
    assert all(p.read_bytes()==b for p,b in before.items())
    assert all(p.stat().st_mode & 0o777==0o644 for p in out.rglob("*") if p.is_file())


def test_portrait_prefixes_resolve_original_frames_and_report_missing_poses(tmp_path):
    root,out=tmp_path/"source",tmp_path/"out"
    source(root)
    header=["#"]+[f"field{i}" for i in range(1,27)];header[21]="Face pics prefix"
    rows=[header]
    for i,prefix in enumerate(["face-","absent-","face-","none"]):
        row=["0"]*27;row[0]=str(i);row[21]=prefix;rows.append(row)
    (root/"Data/Tables/Character portraits.txt").write_text(text(rows))
    palette=bytearray(768);palette[3:6]=bytes((33,66,99))
    lod(root/"Data/mmmerge.icons.lod",{name:wrapped(name,bytes([1])*6,3,2,palette,name_size=16)
        for name in ["face-01","face-02"]},version="MMVI")
    result=run(root,out)
    assert result.returncode==0,result.stderr
    report=json.loads((out/"frame_conversion.json").read_text())
    assert report["portrait_prefixes"]==2 and report["portrait_image_references"]==12
    assert report["portrait_images_imported"]==2 and report["missing_normal_portraits"]==["absent-01"]
    assert len(report["missing_portrait_images"])==10 and "face-03" in report["missing_portrait_images"]
    assert Image.open(out/"engine/icons/face-02.bmp").convert("RGB").getpixel((1,1))==(33,66,99)
    assert not (out/"engine/icons/absent-01.bmp").exists()


@pytest.mark.parametrize("fault",["portrait_duplicate","portrait_continuation","portrait_zero_time",
    "portrait_zero_texture","portrait_time_overflow","first_not_start","marker","icon_order","icon_duplicate",
    "icon_zero_time","icon_name","icon_number","header","padding","ambiguous","damaged_image"])
def test_invalid_frame_profile_or_art_leaves_no_output(tmp_path,fault):
    root,out=tmp_path/"source",tmp_path/"out"
    source(root,fault)
    result=run(root,out)
    assert result.returncode==1 and "Frame conversion failed" in result.stderr
    assert not out.exists()


@pytest.mark.parametrize("victim",["Tables/PFT.txt","Tables/IFT.txt","mm7.icons.lod"])
def test_canonical_input_and_output_boundaries(tmp_path,victim):
    root,out=tmp_path/"source",tmp_path/"out"
    source(root)
    out.mkdir();(out/"keep").write_text("unchanged")
    assert run(root,out).returncode==1 and (out/"keep").read_text()=="unchanged"
    alias=tmp_path/"alias";alias.symlink_to(root,target_is_directory=True)
    assert run(root,alias/"nested").returncode==1 and not (root/"nested").exists()
    path=root/"Data"/victim
    path.rename(tmp_path/"outside");path.symlink_to(tmp_path/"outside")
    result=run(root,tmp_path/"new")
    assert result.returncode==1 and "escapes root" in result.stderr
    assert not (tmp_path/"new").exists()
