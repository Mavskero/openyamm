"""Generated test tones exercise the real FFmpeg/SDL music path, never production assets."""
import json
from pathlib import Path
import shutil
import subprocess

import pytest

TOOL = Path(__file__).parents[1] / "build-apple/music_playback_check"
FFMPEG = shutil.which("ffmpeg")
ENCODERS = subprocess.run([FFMPEG, "-hide_banner", "-encoders"], capture_output=True, text=True).stdout if FFMPEG else ""
pytestmark = pytest.mark.skipif(not TOOL.is_file() or "libmp3lame" not in ENCODERS,
    reason="Build music_playback_check; generated MP3 fixtures require FFmpeg libmp3lame")


def fixture(root, sample_rate, seconds):
    tables = root / "engine/data_tables"
    tables.mkdir(parents=True)
    (tables / "sounds.txt").write_text("fixture\t1\t0\t-\n")
    (tables / "character_speech_events.txt").write_text("SelectCharacter\tFixture\t1\n")
    music = root / "engine/music"
    music.mkdir()
    for track, frequency in [(35, 440), (37, 660)]:
        subprocess.run([FFMPEG, "-v", "error", "-f", "lavfi", "-i",
            f"sine=frequency={frequency}:sample_rate={sample_rate}:duration={seconds}", "-ac", "2",
            "-c:a", "libmp3lame", str(music / f"{track}.mp3")], check=True, capture_output=True, timeout=10)
    return music


@pytest.mark.parametrize("sample_rate", [44100, 48000])
def test_loop_and_complete_resampled_tail(tmp_path, sample_rate):
    music = fixture(tmp_path, sample_rate, 0.8)
    reference = subprocess.check_output([FFMPEG, "-v", "error", "-i", str(music / "35.mp3"),
        "-ar", "48000", "-ac", "2", "-f", "f32le", "pipe:1"], timeout=10)
    result = subprocess.run([str(TOOL), str(tmp_path), "mm6", "loop", "35"],
        capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout)
    assert report["decoded_frames"] == len(reference) // 8
    assert report["pcm_rms"] > 0
    assert report["dummy_output"] and not report["physical_audio_validated"]


def test_pause_pending_track_change_mute_and_decode_race(tmp_path):
    fixture(tmp_path, 44100, 8)
    result = subprocess.run([str(TOOL), str(tmp_path), "mm7", "lifecycle", "35", "37"],
        capture_output=True, text=True, timeout=35)
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout.splitlines()[-1])
    assert report["lifecycle_passed"] and report["playing_postmix_rms"] > 0
