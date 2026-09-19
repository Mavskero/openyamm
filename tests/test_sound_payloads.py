"""Synthetic original IMA WAV wrappers test import, ownership and complete rollback."""
import hashlib
import json
import struct

import pytest

from test_menu_resources import lod, wrapped
from test_sound_resources import prepared, run


def chunk(kind, data):
    return kind + struct.pack("<I", len(data)) + data + (b"\0" if len(data) % 2 else b"")


def wave(predictor=1000, extra=b""):
    fmt = struct.pack("<HHIIHHHH", 17, 1, 22050, 19600, 8, 4, 2, 9)
    data = struct.pack("<hBBI", predictor, 0, 0, 0)
    body = b"WAVE" + chunk(b"fmt ", fmt) + chunk(b"fact", struct.pack("<I", 9)) + chunk(b"data", data) + extra
    return b"RIFF" + struct.pack("<I", len(body)) + body


def setup(prepared):
    root, _, _ = prepared
    base, localized = wave(), wave(3000, chunk(b"LIST", b"abc"))
    lod(root / "Data/mm6.D.lod", {"fountain.wav": wrapped("fountain.wav", base), "ignored.txt": b"ignored"})
    lod(root / "Data/mmmerge.D.lod", {"first shop.wav": wrapped("first shop.wav", base)})
    lod(root / "Data/10 LocZHCN.EnglishD.lod", {"FOUNTAIN.WAV": wrapped("FOUNTAIN.WAV", localized)})
    return base, localized


def snapshot(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob("*") if p.is_file()}


def test_original_bytes_chinese_precedence_and_catalog_coverage(prepared):
    root, _, output = prepared
    base, localized = setup(prepared)
    before = snapshot(root)
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    assert (output / "engine/audio/fountain.wav").read_bytes() == localized
    assert (output / "engine/audio/first shop.wav").read_bytes() == base
    assert snapshot(root) == before
    payloads = json.loads((output / "sound_payloads.json").read_text())
    catalog = json.loads((output / "sound_conversion.json").read_text())
    assert payloads["exported_count"] == catalog["audio_files_exported"] == 2
    assert payloads["override_count"] == 1
    assert payloads["overrides"] == [{"name": "fountain.wav", "selected": "Data/10 LocZHCN.EnglishD.lod",
        "shadowed": "Data/mm6.D.lod"}]
    assert catalog["missing_sample_ids"] == [200, 300]
    assert not payloads["payloads_transcoded"] and not payloads["audio_playback_validated"]
    assert all(p.stat().st_mode & 0o777 == 0o644 for p in output.rglob("*") if p.is_file())


def test_original_mm8_effects_prefixed_voices_and_chinese_layer(prepared):
    root, _, output = prepared
    setup(prepared)
    original, merged, chinese = wave(100), wave(200), wave(300)
    lod(root / "Data/EnglishD.lod", {
        "persistent.wav": wrapped("persistent.wav", original),
        "mm8voice.wav": wrapped("mm8voice.wav", original)})
    lod(root / "Data/mm8.D.lod", {"mm8voice.wav": wrapped("mm8voice.wav", merged)})
    lod(root / "Data/10 LocZHCN.EnglishD.lod", {"mm8voice.wav": wrapped("mm8voice.wav", chinese)})
    before = snapshot(root)
    result = run(prepared)
    assert result.returncode == 0, result.stderr
    assert (output / "engine/audio/persistent.wav").read_bytes() == original
    assert (output / "engine/audio/mm8voice.wav").read_bytes() == chinese
    payloads = json.loads((output / "sound_payloads.json").read_text())
    assert payloads["profile"] == "MMergeOriginalWavZHCNV2"
    assert payloads["overrides"] == [
        {"name": "mm8voice.wav", "selected": "Data/mm8.D.lod", "shadowed": "Data/EnglishD.lod"},
        {"name": "mm8voice.wav", "selected": "Data/10 LocZHCN.EnglishD.lod", "shadowed": "Data/mm8.D.lod"}]
    catalog = json.loads((output / "sound_conversion.json").read_text())
    assert catalog["missing_sample_ids"] == [300]
    assert snapshot(root) == before


def test_original_mm8_does_not_hide_cross_world_name_collision(prepared):
    root, _, output = prepared
    setup(prepared)
    lod(root / "Data/EnglishD.lod", {"fountain.wav": wrapped("fountain.wav", wave())})
    before = snapshot(root)
    result = run(prepared)
    assert result.returncode != 0
    assert "Ambiguous base sound ownership" in result.stderr
    assert not output.exists()
    assert snapshot(root) == before


@pytest.mark.parametrize("fault", ["duplicate_base", "signature", "riff_size", "missing_format", "format",
    "adpcm_block", "channels", "duplicate_fact", "duplicate_data", "empty_fact", "chunk_bounds",
    "truncated_wrapper", "compressed", "symlink", "dangling_link"])
def test_bad_payload_rolls_back_catalog_and_all_sounds(prepared, tmp_path, fault):
    root, _, output = prepared
    base, localized = setup(prepared)
    path = root / "Data/10 LocZHCN.EnglishD.lod"
    raw = bytearray(localized)
    if fault == "duplicate_base":
        lod(root / "Data/mm7.D.lod", {"fountain.wav": wrapped("fountain.wav", base)})
    elif fault == "signature":
        raw[:4] = b"BAD!"
    elif fault == "riff_size":
        struct.pack_into("<I", raw, 4, len(raw) + 50)
    elif fault == "missing_format":
        raw[12:16] = b"junk"
    elif fault == "format":
        struct.pack_into("<H", raw, 20, 2)
    elif fault == "adpcm_block":
        struct.pack_into("<H", raw, 32, 2)
    elif fault == "channels":
        struct.pack_into("<H", raw, 22, 0)
    elif fault == "duplicate_fact":
        raw = bytearray(wave(extra=chunk(b"fact", struct.pack("<I", 9))))
    elif fault == "duplicate_data":
        raw = bytearray(wave(extra=chunk(b"data", bytes(8))))
    elif fault == "empty_fact":
        struct.pack_into("<I", raw, 48, 0)
    elif fault == "chunk_bounds":
        struct.pack_into("<I", raw, 16, 100000)
    member = wrapped("fountain.wav", raw)
    if fault == "truncated_wrapper":
        member = member[:-1]
    elif fault == "compressed":
        member = member[:96] + b"bad zlib data"
    lod(path, {"fountain.wav": member})
    if fault in {"symlink", "dangling_link"}:
        external = tmp_path / "external.lod"
        path.rename(external)
        path.symlink_to(external)
        if fault == "dangling_link":
            external.unlink()
    before = snapshot(root)
    result = run(prepared)
    assert result.returncode != 0, result.stderr
    assert not output.exists()
    assert snapshot(root) == before
