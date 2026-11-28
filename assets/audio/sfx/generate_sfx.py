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


def strike(time, start, frequencies, decay):
    """Short inharmonic modes from the contact surface."""
    offset = np.maximum(time - start, 0)
    modes = sum(
        weight * np.sin(2 * np.pi * frequency * offset)
        for frequency, weight in frequencies
    )
    return modes * env(time, start, decay, 0.00075)


def one_shot(name):
    duration = {"footstep": 0.18, "land": 0.26, "grab": 0.13, "place": 0.22}[name]
    sample_count = round(duration * RATE)
    time = np.arange(sample_count) / RATE
    seed = 4131 + NAMES.index(name)
    contact = colored(sample_count, seed, 620, 3800, 0.20)
    brush = colored(sample_count, seed + 80, 1550, 5300, 0.24)

    if name == "footstep":
        # Two dry sole contacts with a short scuff between them.
        signal = (
            0.43 * contact * env(time, 0.003, 0.013, 0.0008)
            + strike(time, 0.003, ((342, 0.32), (788, 0.19)), 0.012)
        )
        signal += (
            0.17 * brush * env(time, 0.015, 0.023, 0.009)
            + 0.19 * contact * env(time, 0.040, 0.010, 0.0012)
        )
    elif name == "land":
        # Two partially overlapping sole slaps. The low body is a brief layer.
        signal = (
            0.53 * contact * env(time, 0.004, 0.021, 0.0009)
            + strike(time, 0.004, ((235, 0.38), (524, 0.24), (1042, 0.10)), 0.020)
        )
        signal += (
            0.30 * brush * env(time, 0.026, 0.019, 0.0011)
            + strike(time, 0.029, ((328, 0.21), (712, 0.12)), 0.017)
        )
    elif name == "grab":
        # Fabric grip followed by a bright rising pair of mechanical contacts.
        signal = (
            0.21 * brush * env(time, 0.002, 0.011, 0.003)
            + strike(time, 0.010, ((962, 0.34), (2111, 0.21)), 0.017)
            + strike(time, 0.034, ((1292, 0.18), (2681, 0.08)), 0.013)
        )
    else:
        # A sharp set-down followed by a quieter, lower rebound.
        signal = (
            0.38 * contact * env(time, 0.004, 0.012, 0.0011)
            + strike(time, 0.004, ((495, 0.44), (1176, 0.28), (2241, 0.10)), 0.021)
            + strike(time, 0.032, ((319, 0.21), (894, 0.13)), 0.015)
        )

    gate = np.minimum(time / 0.002, 1) * np.minimum(
        (duration - 1 / RATE - time) / 0.018, 1
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
            sample_count, 4175, 220, 2700, 0.30,
            ((590, 115, 0.48), (1210, 180, 0.35), (1810, 270, 0.20)),
        )
        grain = colored(sample_count, 4275, 1450, 4350, 0.30)
        scrape = (
            0.9 + 0.15 * np.sin(2 * np.pi * 6.25 * time)
            + 0.09 * np.cos(2 * np.pi * 13.75 * time + 0.6)
        )
        # Wrapped stick-slip contacts read as friction along a rough surface.
        contacts = np.zeros(sample_count)
        contact_points = (
            (0.043, 0.33), (0.136, 0.17), (0.274, 0.29),
            (0.404, 0.23), (0.556, 0.31), (0.690, 0.21),
        )
        for position, gain in contact_points:
            distance = (time - position + duration * 0.5) % duration - duration * 0.5
            contacts += gain * np.exp(-0.5 * (distance / 0.006) ** 2)
        signal = 0.43 * wood * scrape + (0.14 + contacts) * grain
    else:
        rail = colored(sample_count, 4176, 410, 2100, 0.32)
        bearing = colored(sample_count, 4276, 1340, 4300, 0.36)
        # Quiet low turning tone, with the gearbox and rail audible above it.
        spin = 2 * np.pi * 212 * time + 0.09 * np.sin(2 * np.pi * 3 * time)
        motor = (
            0.22 * np.sin(spin)
            + 0.29 * np.sin(2 * spin + 0.7)
            + 0.11 * np.sin(3 * spin + 0.2)
        )
        chatter = (
            0.60 + 0.28 * np.cos(2 * np.pi * 24 * time)
            + 0.12 * np.sin(2 * np.pi * 11 * time)
        )
        signal = motor + (0.14 * rail + 0.12 * bearing) * chatter

    signal = np.tanh(signal * 1.15)
    signal -= np.mean(signal)
    step = float(signal[0] - signal[-1])
    correction_samples = round(0.003 * RATE)
    curve = np.linspace(0, 1, correction_samples)
    curve = curve * curve * (3 - 2 * curve)
    signal[:correction_samples] -= step * 0.5 * (1 - curve)
    signal[-correction_samples:] += step * 0.5 * curve
    return signal

def make(name, output):
    is_loop = name in ("bookcase", "lift")
    signal = continuous(name) if is_loop else one_shot(name)
    target_peak = {
        "footstep": 0.27,
        "land": 0.36,
        "grab": 0.26,
        "place": 0.30,
        "bookcase": 0.28,
        "lift": 0.26,
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
