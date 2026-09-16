#!/usr/bin/env python3
"""Builds the per-module 00000.mp3/00001.mp3 pairs for the telestories
installation from the raw recordings in to-convert/, per requirements.md's
Code To MP3 Relation table.

- Story files (one per code) are two-pass loudness-normalized to a common
  level, so all five stories sound equally loud regardless of how they were
  originally recorded.
- ring.mp3 is the exception: peak-normalized as loud as possible without
  clipping, not tied to the stories' common loudness level.
- The three 1990-*.wav files are joined in name order first (fade-out,
  500ms silence, fade-in between each pair), then loudness-normalized as
  a single unit like the other stories.
- All outputs are mono, 22050 Hz, 64 kbps CBR MP3.
"""
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "to-convert"
OUT = ROOT / "audio_output"

TARGET_LUFS = -16
TARGET_TP = -1.5
TARGET_LRA = 11
RING_TARGET_PEAK_DB = -1.0  # sample-peak ceiling, 1dB headroom before mp3 encode

# ring.mp3's ring/pause pattern repeats every ~3s (~1.1s ring, ~1.9s pause);
# 8.0s falls inside the silent gap between the 3rd and 4th ring burst
# (measured at 7.18-9.11s), so trimming there lands on true silence - no
# click, and it ends cleanly after 3 full rings rather than cutting one off
# mid-ring.
RING_TRIM_S = 8.0

SAMPLE_RATE = 22050
BITRATE = "64k"

FADE_S = 0.5
PAUSE_S = 2.0  # 500ms original + 1.5s added after listening - felt too short

MONO_DOWNMIX = "pan=mono|c0=0.5*c0+0.5*c1"

# code -> (module number per requirements.md's pin table, source story file)
CODE_TABLE = [
    ("120", 1, "120.wav"),
    ("175", 2, "175.wav"),
    ("177", 3, "177.mp3"),
    ("0900", 4, "0900.wav"),
    ("1990", 5, None),  # built from 1990-1/2/3.wav below
]


def run(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(cmd)}\n{result.stderr}")
    return result.stderr


def ffprobe_duration(path):
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)],
        capture_output=True, text=True, check=True,
    ).stdout.strip()
    return float(out)


def build_joined_1990(out_path):
    segs = [SRC / f"1990-{i}.wav" for i in (1, 2, 3)]
    durs = [ffprobe_duration(s) for s in segs]

    filter_complex = (
        f"[0:a]afade=t=out:st={durs[0] - FADE_S}:d={FADE_S}[a0];"
        f"[1:a]afade=t=in:st=0:d={FADE_S},afade=t=out:st={durs[1] - FADE_S}:d={FADE_S}[a1];"
        f"[2:a]afade=t=in:st=0:d={FADE_S}[a2];"
        f"[a0][3:a][a1][4:a][a2]concat=n=5:v=0:a=1[out]"
    )
    cmd = [
        "ffmpeg", "-y",
        "-i", str(segs[0]), "-i", str(segs[1]), "-i", str(segs[2]),
        "-f", "lavfi", "-t", str(PAUSE_S), "-i", "anullsrc=r=48000:cl=stereo",
        "-f", "lavfi", "-t", str(PAUSE_S), "-i", "anullsrc=r=48000:cl=stereo",
        "-filter_complex", filter_complex,
        "-map", "[out]", str(out_path),
    ]
    run(cmd)
    print(f"  joined 1990-1/2/3 ({durs[0]:.2f}s + {durs[1]:.2f}s + {durs[2]:.2f}s, "
          f"{FADE_S * 1000:.0f}ms fades, {PAUSE_S * 1000:.0f}ms pauses) -> {out_path.name}")


def measure_loudness(input_path, downmix=True):
    prefix = f"{MONO_DOWNMIX}," if downmix else ""
    filter_chain = f"{prefix}loudnorm=I={TARGET_LUFS}:TP={TARGET_TP}:LRA={TARGET_LRA}:print_format=json"
    stderr = run(["ffmpeg", "-y", "-i", str(input_path), "-af", filter_chain, "-f", "null", "-"])
    start, end = stderr.rfind("{"), stderr.rfind("}")
    return json.loads(stderr[start:end + 1])


def normalize_story(input_path, out_path):
    m = measure_loudness(input_path)
    filter_chain = (
        f"{MONO_DOWNMIX},"
        f"loudnorm=I={TARGET_LUFS}:TP={TARGET_TP}:LRA={TARGET_LRA}:"
        f"measured_I={m['input_i']}:measured_TP={m['input_tp']}:"
        f"measured_LRA={m['input_lra']}:measured_thresh={m['input_thresh']}:"
        f"offset={m['target_offset']}:linear=true,"
        f"aresample={SAMPLE_RATE}"
    )
    run([
        "ffmpeg", "-y", "-i", str(input_path), "-af", filter_chain,
        "-ar", str(SAMPLE_RATE), "-ac", "1", "-b:a", BITRATE, "-codec:a", "libmp3lame", "-vn",
        str(out_path),
    ])
    after = measure_loudness(out_path, downmix=False)  # already mono - don't downmix again
    print(f"  {input_path.name}: {float(m['input_i']):.1f} LUFS -> {float(after['input_i']):.1f} LUFS "
          f"-> {out_path}")


def normalize_ring(input_path, out_path):
    tmp_mono = out_path.with_suffix(".tmp.wav")
    run(["ffmpeg", "-y", "-i", str(input_path), "-af", MONO_DOWNMIX, "-t", str(RING_TRIM_S), "-vn", str(tmp_mono)])

    stderr = run(["ffmpeg", "-y", "-i", str(tmp_mono), "-af", "volumedetect", "-f", "null", "-"])
    match = re.search(r"max_volume:\s*(-?[\d.]+) dB", stderr)
    max_volume = float(match.group(1))
    gain = RING_TARGET_PEAK_DB - max_volume

    run([
        "ffmpeg", "-y", "-i", str(tmp_mono), "-af", f"volume={gain}dB,aresample={SAMPLE_RATE}",
        "-ar", str(SAMPLE_RATE), "-ac", "1", "-b:a", BITRATE, "-codec:a", "libmp3lame", "-vn",
        str(out_path),
    ])
    tmp_mono.unlink()
    print(f"  {input_path.name}: peak {max_volume:.1f} dB -> +{gain:.1f} dB gain -> "
          f"peak {RING_TARGET_PEAK_DB:.1f} dB -> {out_path}")


def main():
    OUT.mkdir(exist_ok=True)
    joined_1990 = OUT / "_joined_1990.wav"

    print("Building joined 1990 story...")
    build_joined_1990(joined_1990)

    print("\nNormalizing story files (00001.mp3) to a common loudness level...")
    for code, module_num, story_file in CODE_TABLE:
        module_dir = OUT / f"MP3-{module_num}_code{code}"
        module_dir.mkdir(exist_ok=True)
        src = joined_1990 if story_file is None else SRC / story_file
        normalize_story(src, module_dir / "00001.mp3")

    print("\nNormalizing ring.mp3 (00000.mp3, shared across all modules) to max level without clipping...")
    ring_out = OUT / "ring_00000.mp3"
    normalize_ring(SRC / "ring.mp3", ring_out)
    for code, module_num, _ in CODE_TABLE:
        module_dir = OUT / f"MP3-{module_num}_code{code}"
        (module_dir / "00000.mp3").write_bytes(ring_out.read_bytes())

    joined_1990.unlink()
    print(f"\nDone. Per-module SD card contents are in {OUT}/MP3-<n>_code<code>/")


if __name__ == "__main__":
    main()
