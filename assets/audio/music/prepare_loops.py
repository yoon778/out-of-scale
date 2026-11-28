"""Pack the rendered room loops as mono 32 kHz PCM16 WAVs for XAudio2.

Render each <room>.score.json with music-production into <renders>/<room>/render.
Then run: python prepare_loops.py --renders <renders> --output <game music folder>
Pass --ffmpeg if the installed FFmpeg executable is outside PATH.
"""

import argparse
import hashlib
import json
import os
import subprocess
import wave
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np

ROOMS = ("school", "military", "office", "home")
SAMPLE_RATE = 32000


def ffmpeg_run(ffmpeg, arguments):
    return subprocess.run(
        [ffmpeg, "-hide_banner", "-nostdin", *arguments], capture_output=True, check=True
    )


def dbfs(value):
    return round(float(20 * np.log10(max(value, 1e-12))), 3)


def pack(room, renders, output, ffmpeg):
    source = renders / room / "render" / "loop.wav"
    decoded = ffmpeg_run(ffmpeg, [
        "-i", str(source), "-ac", "1", "-ar", str(SAMPLE_RATE),
        "-af", "aformat=channel_layouts=mono,loudnorm=I=-20:TP=-3:LRA=11:linear=false",
        "-f", "f32le", "-",
    ]).stdout
    samples = np.frombuffer(decoded, dtype="<f4").astype(np.float64)
    if samples.size < 8 * SAMPLE_RATE:
        raise ValueError(f"Loop too short: {source}")

    # Match both ends over a 16 ms cubic correction, keeping the original duration.
    raw_step = float(samples[0] - samples[-1])
    count = round(.016 * SAMPLE_RATE)
    time = np.linspace(0, 1, count)
    blend = time * time * (3 - 2 * time)
    samples[:count] -= raw_step * .5 * (1 - blend)
    samples[-count:] += raw_step * .5 * blend
    peak = float(np.max(np.abs(samples)))
    rms = float(np.sqrt(np.mean(samples ** 2)))
    if not .003 < rms < .4 or not peak < .92:
        raise ValueError(f"Unexpected signal level: {source}, peak={peak:.3f}, RMS={rms:.3f}")

    pcm = np.rint(samples * 32768).astype("<i2")
    assert pcm[0] == pcm[-1]
    final = output / (room + ".wav")
    with wave.open(str(final), "wb") as file:
        file.setparams((1, 2, SAMPLE_RATE, len(pcm), "NONE", "not compressed"))
        file.writeframes(pcm.tobytes())

    # Measure and hash the final encoded wave.
    stderr = ffmpeg_run(ffmpeg, [
        "-i", str(final),
        "-af", "loudnorm=I=-20:TP=-3:LRA=11:print_format=json",
        "-f", "null", os.devnull,
    ]).stderr.decode("utf-8", errors="replace")
    loudness, _ = json.JSONDecoder().raw_decode(stderr[stderr.rfind("{"):])
    return {
        "room": room,
        "sample_rate": SAMPLE_RATE,
        "channels": 1,
        "bits": 16,
        "seconds": len(pcm) / SAMPLE_RATE,
        "rms_dbfs": dbfs(rms),
        "peak_dbfs": dbfs(peak),
        "integrated_lufs": float(loudness["input_i"]),
        "true_peak_dbtp": float(loudness["input_tp"]),
        "clipped_samples": int(np.count_nonzero(np.abs(pcm.astype(np.int32)) >= 32767)),
        "seam_step_lsb": int(pcm[0]) - int(pcm[-1]),
        "raw_join_dbfs": dbfs(abs(raw_step)),
        "sha256": hashlib.sha256(final.read_bytes()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renders", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    with ThreadPoolExecutor(max_workers=len(ROOMS)) as workers:
        results = list(workers.map(
            lambda room: pack(room, args.renders, args.output, args.ffmpeg), ROOMS
        ))
    assert len({result["sha256"] for result in results}) == len(ROOMS)
    report = json.dumps(results, ensure_ascii=False, indent=2)
    (args.renders / "final-wave-proof.json").write_text(report + "\n", encoding="utf-8")
    print(report)


if __name__ == "__main__":
    main()
