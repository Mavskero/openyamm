"""Native VID bounds/ownership and original NPC art, using generated fixtures."""
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import subprocess

from PIL import Image
import pytest

from test_menu_resources import lod, wrapped

TOOL = Path(os.environ.get("MEDIA_IMPORT_TOOL", Path(__file__).parents[1] / "build-apple/media_import"))
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build media_import first")


def vid(entries):
    index, payload = bytearray(), bytearray()
    for name, data in entries:
        index.extend(struct.pack("<40sI", name.encode("ascii"), 4 + 44 * len(entries) + len(payload)))
        payload.extend(data)
    return struct.pack("<I", len(entries)) + index + payload


def fixture(root):
    (root / "Anims").mkdir(parents=True)
    (root / "Data").mkdir()
    # Header-only generated clips test extraction, never pretend to test decoding.
    clips = [("TAV Rich.smk", b"SMK2" + bytes(range(108))), ("church's #1.smk", b"SMK4" + bytes(80))]
    (root / "Anims/mm6.mightdod.vid").write_bytes(vid(clips))
    (root / "Anims/mmmerge.magicdod.vid").write_bytes(vid([("intro.bik", b"BIKi" + bytes(80))]))
    palette = bytearray(768)
    palette[3:6] = bytes((250, 20, 30))
    palette[6:9] = bytes((10, 240, 40))
    for owner, name in [("mm6", "npc0027"), ("mm7", "npc0454"), ("mm8", "npc1038"), ("mmmerge", "npc8042")]:
        bitmap = wrapped(name, bytes([1, 2, 2, 1, 2, 1]), 3, 2, palette, name_size=16)
        lod(root / f"Data/{owner}.icons.lod", {name.upper(): bitmap, "other": b"ignored"}, "MMVI")
    return clips


def run(root, output):
    return subprocess.run([str(TOOL), str(root), str(output)], capture_output=True, text=True, umask=0o077)


def digest(root):
    return {p.relative_to(root): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob("*") if p.is_file()}


def test_raw_video_payloads_and_portrait_world_ownership(tmp_path):
    root, output = tmp_path / "source", tmp_path / "output"
    clips = fixture(root)
    before = digest(root)
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    for name, payload in clips:
        assert (output / "worlds/mm6/videos" / name.lower()).read_bytes() == payload
    report = json.loads((output / "media_conversion.json").read_text())
    assert report["video_count"] == 3 and report["portrait_count"] == 4
    assert not report["runtime_ready"] and not report["playback_validated"]
    assert not report["video_payloads_transcoded"]
    assert "Anims/mm8.magicdod.vid" in report["missing_profile_archives"]
    assert report["videos"][0]["offset"] == 92
    for owner, name in [("worlds/mm6", "npc0027"), ("worlds/mm7", "npc0454"),
                        ("worlds/mm8", "npc1038"), ("engine", "npc8042")]:
        path = output / owner / "icons" / (name + ".bmp")
        image = Image.open(path).convert("RGB")
        assert image.size == (3, 2)
        assert [image.getpixel((x, y)) for y in range(2) for x in range(3)] == [
            (250, 20, 30), (10, 240, 40), (10, 240, 40), (250, 20, 30), (10, 240, 40), (250, 20, 30)]
    assert digest(root) == before
    assert all(stat.S_IMODE(p.stat().st_mode) == 0o644 for p in output.rglob("*") if p.is_file())


def test_original_mm8_videos_case_and_merge_override(tmp_path):
    root, output = tmp_path / "source", tmp_path / "output"
    fixture(root)
    original, replacement = b"SMK2" + bytes(96), b"SMK4" + bytes(120)
    (root / "Anims/Magicdod.vid").write_bytes(vid([("native.smk", original)]))
    (root / "Anims/mightdod.vid").write_bytes(vid([("cquixote.smk", original)]))
    (root / "Anims/mmmerge.mightdod.vid").write_bytes(vid([("cquixote.smk", replacement)]))
    before = digest(root)
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    assert (output / "worlds/mm8/videos/native.smk").read_bytes() == original
    assert (output / "engine/videos/cquixote.smk").read_bytes() == replacement
    assert not (output / "worlds/mm8/videos/cquixote.smk").exists()
    report = json.loads((output / "media_conversion.json").read_text())
    assert report["profile"] == "MMergeNativeMediaV2"
    assert report["video_count"] == len(report["videos"]) == 5
    assert report["video_bytes"] == sum(x["bytes"] for x in report["videos"])
    assert report["video_override_count"] == 1
    assert report["video_overrides"] == [{"name": "cquixote.smk",
        "selected": "Anims/mmmerge.mightdod.vid", "shadowed": "Anims/mightdod.vid"}]
    assert next(x for x in report["videos"] if x["path"].endswith("/native.smk"))["archive"] == "Anims/Magicdod.vid"
    assert digest(root) == before


def test_mm8_prefixed_video_overrides_its_base(tmp_path):
    root, output = tmp_path / "source", tmp_path / "output"
    fixture(root)
    (root / "Anims/mightdod.vid").write_bytes(vid([("native.smk", b"SMK2" + bytes(80))]))
    payload = b"SMK4" + bytes(96)
    (root / "Anims/mm8.mightdod.vid").write_bytes(vid([("native.smk", payload)]))
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    assert (output / "worlds/mm8/videos/native.smk").read_bytes() == payload
    report = json.loads((output / "media_conversion.json").read_text())
    assert report["video_override_count"] == 1


def test_original_video_cannot_shadow_another_world(tmp_path):
    root, output = tmp_path / "source", tmp_path / "output"
    fixture(root)
    (root / "Anims/mightdod.vid").write_bytes(vid([("TAV Rich.smk", b"SMK2" + bytes(80))]))
    before = digest(root)
    result = run(root, output)
    assert result.returncode == 1
    assert "Ambiguous native video ownership" in result.stderr
    assert not output.exists()
    assert digest(root) == before


@pytest.mark.parametrize("fault", ["short_header", "huge_count", "index_offset", "end_offset", "overlap",
    "duplicate", "traversal", "unterminated", "signature", "extension", "short_payload",
    "video_owner", "portrait_owner", "damaged_portrait"])
def test_malformed_or_ambiguous_media_rolls_back(tmp_path, fault):
    root, output = tmp_path / "source", tmp_path / "output"
    fixture(root)
    path = root / "Anims/mm6.mightdod.vid"
    data = bytearray(path.read_bytes())
    if fault == "short_header":
        data = data[:3]
    elif fault == "huge_count":
        struct.pack_into("<I", data, 0, 0xFFFFFFFF)
    elif fault == "index_offset":
        struct.pack_into("<I", data, 44, 91)
    elif fault == "end_offset":
        struct.pack_into("<I", data, 44, len(data))
    elif fault == "overlap":
        struct.pack_into("<I", data, 88, 92)
    elif fault == "duplicate":
        data[48:88] = b"tav rich.SMK".ljust(40, b"\0")
    elif fault == "traversal":
        data[4:44] = b"../escape.smk".ljust(40, b"\0")
    elif fault == "unterminated":
        data[4:44] = b"x" * 36 + b".smk"
    elif fault == "signature":
        data[92:96] = b"BIKi"
    elif fault == "extension":
        data[4:44] = b"movie.exe".ljust(40, b"\0")
    elif fault == "short_payload":
        data = bytearray(vid([("x.smk", b"SM")]))
    elif fault == "video_owner":
        (root / "Anims/mm7.magicdod.vid").write_bytes(data)
    elif fault == "portrait_owner":
        (root / "Data/mm7.icons.lod").write_bytes((root / "Data/mm6.icons.lod").read_bytes())
    elif fault == "damaged_portrait":
        lod(root / "Data/mm6.icons.lod", {"npc0027": b"invalid image"}, "MMVI")
    path.write_bytes(data)
    before = digest(root)
    assert run(root, output).returncode == 1
    assert not output.exists()
    assert digest(root) == before


def test_index_order_does_not_define_payload_order(tmp_path):
    root, output = tmp_path / "source", tmp_path / "output"
    clips = fixture(root)
    path = root / "Anims/mm6.mightdod.vid"
    data = path.read_bytes()
    path.write_bytes(data[:4] + data[48:92] + data[4:48] + data[92:])
    assert run(root, output).returncode == 0
    for name, payload in clips:
        assert (output / "worlds/mm6/videos" / name.lower()).read_bytes() == payload


def test_no_overwrite_source_or_symlink_escape(tmp_path):
    root = tmp_path / "source"
    fixture(root)
    existing = tmp_path / "existing"
    existing.mkdir()
    (existing / "keep").write_text("keep")
    assert run(root, existing).returncode == 1
    assert run(root, root / "output").returncode == 1
    assert (existing / "keep").read_text() == "keep"
    path = root / "Anims/mm6.mightdod.vid"
    external = tmp_path / "external.vid"
    path.rename(external)
    path.symlink_to(external)
    assert run(root, tmp_path / "output").returncode == 1
    assert not (tmp_path / "output").exists()


def music_fixture(root):
    directory = root / "Music"
    directory.mkdir(parents=True)
    # Only signature/extraction fixtures; these are not playable audio.
    tracks = {"35.mp3": b"ID3" + bytes(90), "37.MP3": b"\xff\xfb\xe0" + bytes(100)}
    for name, data in tracks.items():
        (directory / name).write_bytes(data)
    return tracks


def test_numeric_music_is_shared_and_byte_identical(tmp_path):
    root, output = tmp_path / "source", tmp_path / "output"
    tracks = music_fixture(root)
    before = digest(root)
    result = run(root, output)
    assert result.returncode == 0, result.stderr
    report = json.loads((output / "media_conversion.json").read_text())
    assert report["music_count"] == 2
    assert report["music_bytes"] == sum(map(len, tracks.values()))
    assert not report["music_payloads_transcoded"] and not report["missing_music_directory"]
    assert [t["track"] for t in report["music"]] == [35, 37]
    for name, data in tracks.items():
        path = output / "engine/music" / name.lower()
        assert path.read_bytes() == data
        assert stat.S_IMODE(path.stat().st_mode) == 0o644
    assert digest(root) == before


def test_absent_music_is_explicit(tmp_path):
    root, output = tmp_path / "source", tmp_path / "output"
    fixture(root)
    assert run(root, output).returncode == 0
    report = json.loads((output / "media_conversion.json").read_text())
    assert report["missing_music_directory"] and report["music_count"] == 0


@pytest.mark.parametrize("fault", ["empty", "signature", "oversize", "leading_zero", "non_numeric",
    "zero", "too_long", "directory", "file_escape", "directory_escape", "dangling_link"])
def test_invalid_music_rolls_back_all_media(tmp_path, fault):
    root, output = tmp_path / "source", tmp_path / "output"
    fixture(root)
    music_fixture(root)
    path = root / "Music/37.MP3"
    if fault == "empty":
        path.write_bytes(b"")
    elif fault == "signature":
        path.write_bytes(b"unrecognised payload")
    elif fault == "oversize":
        with path.open("wb") as stream:
            stream.truncate(128 * 1024 * 1024 + 1)
    elif fault in {"leading_zero", "non_numeric", "zero", "too_long"}:
        path.rename(path.with_name({"leading_zero": "037.mp3", "non_numeric": "music.mp3",
            "zero": "0.mp3", "too_long": "1000000.mp3"}[fault]))
    elif fault == "directory":
        path.unlink()
        path.mkdir()
    elif fault in {"file_escape", "dangling_link"}:
        external = tmp_path / "external.mp3"
        path.rename(external)
        path.symlink_to(external)
        if fault == "dangling_link":
            external.unlink()
    elif fault == "directory_escape":
        directory = root / "Music"
        external = tmp_path / "external_music"
        directory.rename(external)
        directory.symlink_to(external, target_is_directory=True)
    # Don't dereference dangling links in the source-preservation snapshot.
    before = digest(root)
    result = run(root, output)
    assert result.returncode == 1, result.stderr
    assert not output.exists()
    assert digest(root) == before
