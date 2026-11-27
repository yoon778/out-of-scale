"""Generate original Out of Scale SFX with deterministic NumPy synthesis.

Dependency: NumPy, available in the configured music-production Python runtime.
Run: python generate_sfx.py --output <directory> --proof <temp.json>

No external samples, music models, network, playback, or required attribution.
The WAVs are mono 32000 Hz PCM16 RIFF; loops repeat their whole file.
"""

import argparse
import hashlib
import json
import math
import wave
from pathlib import Path

import numpy as np

RATE = 32000
NAMES = ("footstep", "land", "grab", "place", "bookcase", "lift")


def colored(sample_count, seed, low, high, tilt=0.4, resonances=()):
    rng = np.random.default_rng(seed)
    frequencies = np.fft.rfftfreq(sample_count, 1 / RATE)
    spectrum = np.fft.rfft(rng.standard_normal(sample_count))

    # Soft, zero-phase spectral filtering; a DFT-periodic process for the loops.
    band = (1 - np.exp(-(frequencies / max(low, 1)) ** 4)) * np.exp(
        -(frequencies / high) ** 4
    )
    band *= np.maximum(frequencies, low) ** (-tilt)
    for frequency, width, gain in resonances:
        band *= 1 + gain * np.exp(
            -0.5 * ((frequencies - frequency) / width) ** 2
        )
    band[0] = 0
    signal = np.fft.irfft(spectrum * band, sample_count)
    return signal / max(float(np.sqrt(np.mean(signal * signal))), 1e-9)


def env(time, start, decay, attack=0.002):
    offset = np.maximum(time - start, 0)
    return (time >= start) * (1 - np.exp(-offset / attack)) * np.exp(
        -offset / decay
    )


def thump(time, start, start_frequency, end_frequency, decay):
    offset = np.maximum(time - start, 0)
    phase = 2 * np.pi * (
        end_frequency * offset
        + (start_frequency - end_frequency)
        * 0.016
        * (1 - np.exp(-offset / 0.016))
    )
    return np.sin(phase) * env(time, start, decay)


def one_shot(name):
    duration = {"footstep": 0.18, "land": 0.26, "grab": 0.13, "place": 0.22}[name]
    sample_count = round(duration * RATE)
    time = np.arange(sample_count) / RATE
    seed = 4001 + NAMES.index(name)
    low_noise = colored(sample_count, seed, 50, 1100, 0.7)
    mid_noise = colored(sample_count, seed + 80, 450, 3400, 0.2)

    if name == "footstep":
        signal = (
            0.70 * thump(time, 0.008, 155, 86, 0.028)
            + 0.30 * low_noise * env(time, 0.009, 0.019)
        )
        signal += (
            0.34 * thump(time, 0.066, 225, 145, 0.019)
            + 0.15 * mid_noise * env(time, 0.063, 0.013, 0.004)
        )
    elif name == "land":
        signal = (
            0.83 * thump(time, 0.007, 112, 61, 0.043)
            + 0.37 * low_noise * env(time, 0.010, 0.032)
        )
        signal += (
            0.56 * thump(time, 0.077, 150, 83, 0.031)
            + 0.22 * mid_noise * env(time, 0.075, 0.021, 0.003)
        )
    elif name == "grab":
        signal = (
            0.28 * low_noise * env(time, 0.006, 0.020, 0.004)
            + 0.24 * mid_noise * env(time, 0.011, 0.024, 0.005)
        )
        signal += (
            0.09 * thump(time, 0.018, 420, 290, 0.016)
            + 0.12 * mid_noise * env(time, 0.055, 0.013, 0.004)
        )
    else:
        signal = (
            0.73 * thump(time, 0.009, 230, 115, 0.030)
            + 0.35 * low_noise * env(time, 0.009, 0.018)
        )
        signal += (
            0.17 * thump(time, 0.064, 330, 185, 0.015)
            + 0.10 * mid_noise * env(time, 0.060, 0.014, 0.003)
        )

    gate = np.minimum(time / 0.004, 1) * np.minimum(
        (duration - 1 / RATE - time) / 0.025, 1
    )
    gate = np.clip(gate, 0, 1)
    gate = gate * gate * (3 - 2 * gate)
    signal *= gate
    signal -= float(np.mean(signal)) / float(np.mean(gate)) * gate
    signal[0] = signal[-1] = 0
    return signal


def continuous(name):
    duration = 0.8 if name == "bookcase" else 1.0
    sample_count = round(duration * RATE)
    time = np.arange(sample_count) / RATE

    if name == "bookcase":
        wood = colored(
            sample_count, 4005, 70, 1700, 0.72,
            ((170, 45, 1.3), (355, 75, 0.8), (660, 150, 0.45)),
        )
        grain = colored(sample_count, 4105, 1300, 3800, 0.2)
        pulse = (
            1 + 0.12 * np.sin(2 * np.pi * 5 * time)
            + 0.06 * np.cos(2 * np.pi * 11.25 * time + 0.6)
        )
        # Wrapped stick-slip contacts, irregular within one 0.8-second turn.
        contacts = np.zeros(sample_count)
        contact_points = (
            (0.047, 0.20), (0.160, 0.15), (0.284, 0.23),
            (0.392, 0.16), (0.551, 0.19), (0.699, 0.13),
        )
        for position, gain in contact_points:
            distance = (time - position + duration * 0.5) % duration - duration * 0.5
            contacts += gain * np.exp(-0.5 * (distance / 0.008) ** 2)
        signal = (
            0.64 * wood * pulse
            + 0.12 * grain * (0.3 + contacts)
            + contacts * colored(sample_count, 4205, 250, 2100, 0.3)
        )
    else:
        rail = colored(
            sample_count, 4006, 110, 1550, 0.6,
            ((260, 70, 0.7), (540, 120, 0.35)),
        )
        bearing = colored(sample_count, 4106, 1000, 3300, 0.4)
        motor = (
            0.42 * np.sin(2 * np.pi * 70 * time + 0.4)
            + 0.15 * np.sin(2 * np.pi * 140 * time + 0.7)
        )
        motor += 0.065 * np.sin(2 * np.pi * 210 * time + 0.1)
        fluctuation = (
            1 + 0.055 * np.cos(2 * np.pi * 2 * time)
            + 0.025 * np.sin(2 * np.pi * 7 * time)
        )
        signal = (motor + 0.27 * rail) * fluctuation + 0.045 * bearing

    signal = np.tanh(signal * 1.3)
    signal -= np.mean(signal)
    # Close both endpoints with a 4 ms cubic correction, not a fade/silent gap.
    step = float(signal[0] - signal[-1])
    correction_samples = round(0.004 * RATE)
    curve = np.linspace(0, 1, correction_samples)
    curve = curve * curve * (3 - 2 * curve)
    signal[:correction_samples] -= step * 0.5 * (1 - curve)
    signal[-correction_samples:] += step * 0.5 * curve
    return signal


def make(name, output):
    is_loop = name in ("bookcase", "lift")
    signal = continuous(name) if is_loop else one_shot(name)
    target_peak = {
        "footstep": 0.29,
        "land": 0.42,
        "grab": 0.24,
        "place": 0.34,
        "bookcase": 0.38,
        "lift": 0.34,
    }[name]
    signal *= target_peak / float(np.max(np.abs(signal)))
    pcm = np.rint(signal * 32767).astype("<i2")
    final = output / (name + ".wav")
    with wave.open(str(final), "wb") as writer:
        writer.setnchannels(1)
        writer.setsampwidth(2)
        writer.setframerate(RATE)
        writer.writeframes(pcm.tobytes())

    samples = pcm.astype(np.float64) / 32768
    rms = float(np.sqrt(np.mean(samples * samples)))
    data = final.read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE" and data[12:16] == b"fmt "
    assert abs(int(pcm[0]) - int(pcm[-1])) == 0
    assert np.max(np.abs(pcm.astype(np.int32))) < 32767
    measured_peak = float(np.max(np.abs(samples)))
    return {
        "file": str(final),
        "sample_rate": RATE,
        "channels": 1,
        "bits": 16,
        "seconds": len(pcm) / RATE,
        "frames": len(pcm),
        "bytes": len(data),
        "loop": is_loop,
        "peak": measured_peak,
        "peak_dbfs": round(20 * math.log10(measured_peak), 3),
        "rms_dbfs": round(20 * math.log10(rms), 3),
        "dc": float(np.mean(samples)),
        "first_lsb": int(pcm[0]),
        "last_lsb": int(pcm[-1]),
        "seam_step_lsb": int(pcm[0]) - int(pcm[-1]),
        "clipped_samples": int(np.sum(np.abs(pcm.astype(np.int32)) >= 32767)),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--proof", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    records = [make(name, args.output) for name in NAMES]
    assert len({record["sha256"] for record in records}) == 6
    text = json.dumps(records, indent=2)
    if args.proof:
        args.proof.write_text(text, encoding="utf-8")
    print(text)
