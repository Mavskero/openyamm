"""Complete IMA sample frames in partial final blocks must survive the shared WAV decoder."""
import json
from pathlib import Path
import struct
import subprocess

import pytest

from test_sound_payloads import chunk

TOOL = Path(__file__).parents[1] / "build-apple/sound_playback_check"
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build sound_playback_check first")


@pytest.mark.parametrize("channels,full_blocks", [(1, 0), (1, 1), (2, 0), (2, 1)])
def test_partial_ima_blocks_keep_complete_frames(tmp_path, channels, full_blocks):
    block_size = 512 * channels
    block_frames = 1017
    # A mono subblock has 8 sample frames; stereo stores matching four-byte chunks per channel.
    header = b"".join(struct.pack("<hBB", 2000, 0, 0) for _ in range(channels))
    full = header + bytes(block_size - len(header))
    partial = header + bytes(400 * channels)
    source_frames = full_blocks * block_frames + 801
    fmt = struct.pack("<HHIIHHHH", 17, channels, 22050, 11100 * channels, block_size, 4, 2, block_frames)
    body = b"WAVE" + chunk(b"fmt ", fmt) + chunk(b"fact", struct.pack("<I", source_frames))
    body += chunk(b"data", full * full_blocks + partial)
    directory = tmp_path / "engine/audio"
    directory.mkdir(parents=True)
    (directory / "partial.wav").write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)
    result = subprocess.run([str(TOOL), str(tmp_path), "mm6", "decode"],
        capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout)
    assert abs(report["decoded_stereo_frames"] - source_frames * 48000 / 22050) < 1
    assert report["pcm_rms"] > 0.05
