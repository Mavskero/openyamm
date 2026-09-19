from __future__ import annotations

from pathlib import Path
import struct
import sys
import zlib

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/mmerge_import"))
from chinese_text import decode_legacy_text, remaining_controls
from legacy_lod import ImportFormatError, LodArchive, inflate_exact
from import_source import stage_source


def make_lod(path, name=b"global.txt", text=b"test", compressed=False, version=b"MMVIII"):
    name_size, stride, header_size = (64, 76, 96) if version == b"MMVIII" else (16, 32, 48)
    payload = zlib.compress(text) if compressed else text
    member = bytearray(header_size)
    member[:len(name)] = name
    struct.pack_into("<I", member, name_size + 4, len(payload))
    struct.pack_into("<I", member, name_size + 24, len(text) if compressed else 0)
    member.extend(payload)
    data = bytearray(288 + stride)
    data[:4] = b"LOD\0"
    data[4:4 + len(version)] = version
    struct.pack_into("<IIII", data, 272, 288, stride + len(member), 0, 1)
    data[288:288 + len(name)] = name
    struct.pack_into("<II", data, 288 + name_size, stride, len(member))
    data.extend(member)
    path.write_bytes(data)


@pytest.mark.parametrize("version", [b"MMVI", b"MMVIII"])
@pytest.mark.parametrize("compressed", [False, True])
def test_language_resource_round_trip(tmp_path, version, compressed):
    path = tmp_path / "language.lod"
    original = "0\t防御\r\n1\t精确\0".encode("gb2312")
    make_lod(path, text=original, version=version, compressed=compressed)
    archive = LodArchive(path)
    assert archive.read_text_payload(archive.entries[0]) == original


def test_traversal_is_rejected(tmp_path):
    path = tmp_path / "bad.lod"
    make_lod(path, name=b"../bad.txt")
    with pytest.raises(ImportFormatError, match="Unsafe"):
        LodArchive(path)


def test_truncated_archive_is_rejected(tmp_path):
    path = tmp_path / "bad.lod"
    make_lod(path)
    path.write_bytes(path.read_bytes()[:-1])
    with pytest.raises(ImportFormatError, match="bounds"):
        LodArchive(path)


def test_member_cannot_point_into_index(tmp_path):
    path = tmp_path / "bad.lod"
    make_lod(path)
    data = bytearray(path.read_bytes())
    struct.pack_into("<I", data, 352, 0)
    path.write_bytes(data)
    with pytest.raises(ImportFormatError, match="bounds"):
        LodArchive(path)


@pytest.mark.parametrize("payload,size", [(zlib.compress(b"abc"), 2),
    (zlib.compress(b"abc") + b"junk", 3), (zlib.compress(b"abc")[:-1], 3)])
def test_bad_compression_boundaries_are_rejected(payload, size):
    with pytest.raises(ImportFormatError):
        inflate_exact(payload, size)


def test_chinese_glyph_runs_restore_text_and_keep_other_controls():
    wrapped = "ID\t\x0e \x0e魔\x07 \x0e法\x07 \x0e门\x07\x0f 6\x0c12345\r\n"
    text, count = decode_legacy_text(wrapped.encode("gb2312"), "gb2312")
    assert text == "ID\t魔法门 6\x0c12345\n"
    assert count == 3
    assert remaining_controls(text) == {"0x0c": 1}


def test_incomplete_glyph_run_is_reported_not_silently_removed():
    data = "\x0e \x0e魔\x07".encode("gb2312")
    text, count = decode_legacy_text(data, "gb2312")
    assert count == 0
    assert remaining_controls(text) == {"0x07": 1, "0x0e": 2}


def test_invalid_encoding_is_not_replaced():
    with pytest.raises(UnicodeDecodeError):
        decode_legacy_text(b"\xff\xff", "gb2312")


def test_source_cannot_be_output(tmp_path):
    (tmp_path / "Data").mkdir()
    with pytest.raises(ValueError, match="non-overlapping"):
        stage_source(tmp_path, tmp_path / "converted")


def test_import_records_missing_sources_and_preserves_original(tmp_path):
    source = tmp_path / "game"
    (source / "Data").mkdir(parents=True)
    make_lod(source / "Data/mmmerge.T.lod")
    original = (source / "Data/mmmerge.T.lod").read_bytes()
    output = tmp_path / "output"
    result = stage_source(source, output)
    assert not result["audit_ok"] and not result["runtime_ready"]
    assert result["missing_mm8_base_archives"] == ["Data/bitmaps.lod", "Data/sprites.lod", "Data/icons.lod"]
    assert result["archives"][0]["texts"][0]["name"] == "global.txt"
    assert (source / "Data/mmmerge.T.lod").read_bytes() == original
    assert (output / "archives/Data/mmmerge.T.lod/global.txt").read_text() == "test"
    with pytest.raises(ValueError, match="already exists"):
        stage_source(source, output)
