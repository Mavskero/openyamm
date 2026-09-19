"""Generated OGV clips exercise the shared loop clock without game assets."""
import json
import os
from pathlib import Path
import shutil
import subprocess

import pytest

TOOL = Path(__file__).parents[1] / "build-apple/media_playback_check"
FFMPEG = os.environ.get("MEDIA_TEST_FFMPEG", shutil.which("ffmpeg"))
ENCODERS = subprocess.run([FFMPEG, "-hide_banner", "-encoders"], capture_output=True, text=True).stdout if FFMPEG else ""
pytestmark = pytest.mark.skipif(not TOOL.is_file() or "libtheora" not in ENCODERS or "libvorbis" not in ENCODERS,
    reason="Build media_playback_check; generated clips require FFmpeg libtheora/libvorbis encoders")


def make_clip(root, audio):
    directory = root / "engine/videos/houses"
    directory.mkdir(parents=True)
    args = [FFMPEG, "-v", "error", "-f", "lavfi", "-i", "color=c=blue:size=32x32:rate=10:duration=1.5"]
    if audio:
        args += ["-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=0.5", "-c:a", "libvorbis"]
    args += ["-c:v", "libtheora", "-pix_fmt", "yuv420p", str(directory / "generated.ogv")]
    subprocess.run(args, check=True, capture_output=True, timeout=15)


@pytest.mark.parametrize("audio", [False, True])
def test_ogv_loops_with_silence_or_a_shorter_audio_track(tmp_path, audio):
    make_clip(tmp_path, audio)
    result = subprocess.run([str(TOOL), str(tmp_path), "mm6", "generated", "Videos/Houses", "4.2", "loop",
                             "audio" if audio else "silent"], capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout)
    assert report["passed"] and not report["finished"]
    assert report["last_frame_seconds"] >= 4.2
    assert report["maximum_frame_gap"] < 0.15
    assert report["frames_presented"] >= 40
    assert (report["audio_frames_queued"] > 0) == audio
