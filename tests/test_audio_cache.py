"""Idle PCM eviction must preserve actual playing, paused, preloaded and registered audio."""
from pathlib import Path
import struct
import subprocess
import wave

import pytest

TOOL = Path(__file__).parents[1] / "build-apple/audio_cache_check"
pytestmark = pytest.mark.skipif(not TOOL.is_file(), reason="Build audio_cache_check first")


@pytest.mark.parametrize("mode", ["lru", "active", "pinned", "natural-end"])
def test_cache_lifetime(mode, tmp_path):
    directory = tmp_path / "engine/audio"
    directory.mkdir(parents=True)
    for index, name in enumerate(["a", "b", "c"], 1):
        with wave.open(str(directory / f"{name}.wav"), "wb") as stream:
            stream.setparams((2, 2, 48000, 1024, "NONE", "not compressed"))
            stream.writeframes(struct.pack("<hh", 1000 * index, 1000 * index) * 1024)
    result = subprocess.run([str(TOOL), str(tmp_path), mode], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
