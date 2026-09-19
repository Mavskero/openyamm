"""House presentation joins original movie/house/NPC identities with translated tables."""
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess

import pytest

from test_core_resources import archive, rows, text

TOOL = Path(os.environ.get("HOUSE_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/house_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build house_import first")


def fixture(tmp_path, fault=None):
    root, base = tmp_path / "source", tmp_path / "base"
    (root / "Data/Tables").mkdir(parents=True)
    tables = base / "engine/data_tables"
    tables.mkdir(parents=True)
    movie_rows = [["#", "FileName", "NPCPic", "HouseType", "Sounds"],
                  ["394", "human tavern 01", "27", "Tavern", "139"],
                  ["395", "", "0", "House", "0"]]
    header = ["#", "#", "Type", "Map", "Picture", "Name"] + [""] * 18
    house_rows = [["2D Events by Type"] + [""] * 23, header]
    for id_, name, movie in [(260, "Tavern", 394), (261, "Home", 395), (1089, "", 395)]:
        row = [str(id_), "1", "Tavern", "151", str(movie), name, "Owner", "Title", "13"] + ["0"] * 15
        house_rows.append(row)
    npc_rows = [["NPC Data (Special)"], ["#", "Name"]]
    for id_, house_id in [(787, 260), (786, 260), (790, 0), (765, 1089)]:
        npc_rows.append([str(id_), f"NPC {id_}", "372", "0", "0", "0", str(house_id), "0", "1", "0",
                         "1314", "1609", "0", "0", "0", "0", "Notes"])
    converted_houses = [r.copy() for r in house_rows[2:] if r[5]]
    converted_houses[0][5] = "独行侠酒馆"
    converted_npcs = [r.copy() for r in npc_rows[2:]]
    converted_npcs[1][1] = "安多弗"
    if fault == "duplicate_movie":
        movie_rows.append(movie_rows[1])
    elif fault == "missing_movie":
        movie_rows.pop(1)
    elif fault == "unsafe_video":
        movie_rows[1][1] = "../movie"
    elif fault == "npc_moved":
        converted_npcs[1][6] = "261"
    elif fault == "house_changed":
        converted_houses[0][3] = "152"
    elif fault == "house_animation_changed":
        converted_houses[0][4] = "395"
    elif fault == "unknown_house":
        npc_rows[2][6] = converted_npcs[0][6] = "999"
    elif fault == "short_house":
        converted_houses[0] = converted_houses[0][:5]
    elif fault == "sound_group_overflow":
        movie_rows[1][4] = "256"
    archive(root / "Data/mmmerge.T.lod", {"2devents.txt": house_rows, "npcdata.txt": npc_rows})
    (root / "Data/Tables/House Movies.txt").write_text(text(movie_rows))
    (tables / "house_data.txt").write_text(text(converted_houses))
    (tables / "npc.txt").write_text(text(converted_npcs))
    sounds = [["#SoundCatalogMode", "MM8OrderedV1"], ["Name", "Id", "Type", "Is3D"],
              ["null", "0", "system", "-"], ["tavern_greeting", "31391", "swap", "-"],
              ["different_original_game_sound", "43901", "swap", "-"]]
    if fault == "missing_sound":
        sounds.pop(3)
    (tables / "sounds.txt").write_text(text(sounds))
    return root, base


def run(root, base, output):
    return subprocess.run([str(TOOL), str(root), str(base), str(output)],
                          capture_output=True, text=True, umask=0o077)


def digest_tree(root):
    return {p.relative_to(root): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob("*") if p.is_file()}


def test_localized_houses_movie_portraits_residents_and_merged_sound_ids(tmp_path):
    root, base = fixture(tmp_path)
    before = digest_tree(root), digest_tree(base)
    output = tmp_path / "output"
    result = run(root, base, output)
    assert result.returncode == 0, result.stderr
    mapping = {r[0]: r for r in rows(output / "engine/data_tables/house_animations.txt") if r[0].isdigit()}
    assert mapping["260"] == ["260", "394", "独行侠酒馆", "786,787", "human tavern 01", "", "0",
                              "31390", "27", "Tavern"]
    assert mapping["261"][3:9] == ["", "", "", "0", "0", "0"]
    assert set(mapping) == {"260", "261"}
    report = json.loads((output / "house_conversion.json").read_text())
    assert report["houses"] == 2 and report["residents"] == 2 and report["resident_houses"] == 1
    assert report["inactive_house_npcs"] == [765] and report["missing_greeting_sound_ids"] == []
    assert report["typed_validation"] is True and report["runtime_ready"] is False
    assert report["video_payloads_converted"] is False
    assert (digest_tree(root), digest_tree(base)) == before
    assert all(stat.S_IMODE(p.stat().st_mode) == 0o644 for p in output.rglob("*") if p.is_file())


@pytest.mark.parametrize("fault", ["duplicate_movie", "missing_movie", "unsafe_video", "npc_moved",
                                  "house_changed", "house_animation_changed", "unknown_house", "short_house",
                                  "sound_group_overflow"])
def test_rejects_broken_joins_without_publishing_partial_output(tmp_path, fault):
    root, base = fixture(tmp_path, fault)
    before = digest_tree(root), digest_tree(base)
    output = tmp_path / "output"
    assert run(root, base, output).returncode != 0
    assert not output.exists()
    assert (digest_tree(root), digest_tree(base)) == before


def test_missing_sound_is_reported_without_substituting_another_group(tmp_path):
    root, base = fixture(tmp_path, "missing_sound")
    output = tmp_path / "output"
    result = run(root, base, output)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / "house_conversion.json").read_text())
    assert report["missing_greeting_sound_ids"] == [31391]
    mapping = rows(output / "engine/data_tables/house_animations.txt")
    assert next(r for r in mapping if r[0] == "260")[7] == "31390"


def test_protects_existing_output_input_roots_and_original_symlink_escape(tmp_path):
    root, base = fixture(tmp_path)
    existing = tmp_path / "existing"
    existing.mkdir()
    marker = existing / "keep"
    marker.write_text("untouched")
    for output in (existing, root / "output", base / "output"):
        assert run(root, base, output).returncode != 0
    assert marker.read_text() == "untouched"
    movie = root / "Data/Tables/House Movies.txt"
    external = tmp_path / "external.txt"
    movie.rename(external)
    movie.symlink_to(external)
    assert run(root, base, tmp_path / "output").returncode != 0
