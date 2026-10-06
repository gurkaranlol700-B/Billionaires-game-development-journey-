"""SEAWALL -- synthesise the sounds no microphone can record for us.

Recordings give us the real world: feet, cloth, doors, wind. These are the other
half of horror audio -- the sounds that are felt more than heard, and that have to
be built from maths because they do not exist as objects:

    heartbeat   the player's own pulse, three intensities, felt in the chest
    tinnitus    the ringing after a scare or a hard landing
    subdrone    30-40 Hz bed that makes a room feel wrong before anything happens
    cluster     dissonant pad, the "something is here" layer
    riser       Shepard tone -- rises forever without ever getting higher (dizziness)
    stinger     the hit itself: sub boom + metal screech + noise slam
    bodythud    the low half of a hard landing, layered under a real footstep
    arc         electrical crackle for the broken fluorescent fixture

    python synth_layers.py --list
    python synth_layers.py all -o "D:/UnrealProjects/Seawall/Art/Audio/Source/Generated"
    python synth_layers.py heartbeat riser -o <dir>

Everything is written mono 48 kHz 16-bit, which is what the game uses. Seeded, so
re-running produces identical files -- change --seed to get new variations.

Needs numpy + scipy:
  C:\\Users\\Intel\\AppData\\Local\\Programs\\Python\\Python310\\python.exe
"""
from __future__ import annotations

import argparse
import os

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter, sosfilt

RATE = 48000
rng = np.random.default_rng(7)


# --- helpers ---------------------------------------------------------------

def t(seconds: float) -> np.ndarray:
    return np.linspace(0.0, seconds, int(RATE * seconds), endpoint=False)


def noise(seconds: float) -> np.ndarray:
    return rng.normal(0.0, 1.0, int(RATE * seconds))


def lowpass(x: np.ndarray, hz: float, order: int = 4) -> np.ndarray:
    b, a = butter(order, min(hz / (RATE / 2), 0.99), btype="low")
    return lfilter(b, a, x)


def highpass(x: np.ndarray, hz: float, order: int = 4) -> np.ndarray:
    b, a = butter(order, max(hz / (RATE / 2), 1e-4), btype="high")
    return lfilter(b, a, x)


def bandpass(x: np.ndarray, lo: float, hi: float, order: int = 4) -> np.ndarray:
    sos = butter(order, [max(lo / (RATE / 2), 1e-4), min(hi / (RATE / 2), 0.99)],
                 btype="band", output="sos")
    return sosfilt(sos, x)


def fade(x: np.ndarray, in_ms: float = 5.0, out_ms: float = 40.0) -> np.ndarray:
    a, b = int(RATE * in_ms / 1000), int(RATE * out_ms / 1000)
    y = x.copy()
    if len(y) > a + b:
        y[:a] *= np.linspace(0, 1, a)
        y[-b:] *= np.linspace(1, 0, b)
    return y


def norm(x: np.ndarray, peak_dbfs: float = -3.0) -> np.ndarray:
    p = np.abs(x).max()
    return x * ((10 ** (peak_dbfs / 20)) / p) if p > 0 else x


def loopable(x: np.ndarray, crossfade_s: float = 1.0) -> np.ndarray:
    """Crossfade the tail over the head so the loop has no seam."""
    n = int(RATE * crossfade_s)
    head, tail = x[:n], x[-n:]
    ramp = np.linspace(0, 1, n)
    return np.concatenate([tail * (1 - ramp) + head * ramp, x[n:-n]])


# --- generators ------------------------------------------------------------

def gen_heartbeat() -> dict[str, np.ndarray]:
    """One beat = lub (louder, lower) then dub. Pitch falls through each thump,
    which is what makes it read as a body rather than a drum machine."""
    out = {}
    for name, (f0, f1, gain, thump_ms) in {
        "calm":    (58.0, 42.0, 0.55, 150),
        "raised":  (64.0, 46.0, 0.80, 130),
        "pounding": (72.0, 50.0, 1.00, 115),
    }.items():
        def thump(ms, level, bright):
            env_t = t(ms / 1000)
            sweep = np.linspace(f0, f1, len(env_t))
            body = np.sin(2 * np.pi * np.cumsum(sweep) / RATE)
            env = np.exp(-env_t * (1000 / ms) * 3.2)
            click = lowpass(noise(ms / 1000), 900) * np.exp(-env_t * 220) * bright
            return (body * env + click * 0.35) * level

        lub = thump(thump_ms, gain, 1.0)
        dub = thump(thump_ms * 0.8, gain * 0.62, 0.7)
        gap = np.zeros(int(RATE * 0.16))
        beat = np.concatenate([lub, gap, dub])
        beat = lowpass(beat, 220)
        out[f"SW_Body_Heartbeat_{name}"] = norm(fade(beat, 2, 60), -6.0)
    return out


def gen_tinnitus() -> dict[str, np.ndarray]:
    """After a scare: a high ring that fades over several seconds, with the
    narrow noise band that makes it sound like ears, not a test tone."""
    dur = 6.0
    ts = t(dur)
    ring = (np.sin(2 * np.pi * 7100 * ts) * 0.6 + np.sin(2 * np.pi * 8650 * ts) * 0.4)
    wobble = 1 + 0.004 * np.sin(2 * np.pi * 0.7 * ts)
    hiss = bandpass(noise(dur), 5500, 9000) * 0.25
    env = np.exp(-ts * 0.55)
    return {"SW_Body_Tinnitus_Ring": norm(fade(( ring * wobble + hiss) * env, 8, 800), -12.0)}


def gen_subdrone() -> dict[str, np.ndarray]:
    """The dread bed. Two detuned sub oscillators beating slowly against each
    other, plus filtered rumble. Loops seamlessly."""
    dur = 30.0
    ts = t(dur)
    a = np.sin(2 * np.pi * 34.0 * ts)
    b = np.sin(2 * np.pi * 34.37 * ts)          # 0.37 Hz beat = slow unease
    c = np.sin(2 * np.pi * 51.0 * ts) * 0.35
    rumble = lowpass(noise(dur), 120) * 0.5
    swell = 0.75 + 0.25 * np.sin(2 * np.pi * 0.045 * ts)
    return {"SW_Tension_SubDrone_Loop": norm(loopable((a + b + c + rumble) * swell), -9.0)}


def gen_cluster() -> dict[str, np.ndarray]:
    """Minor second + tritone cluster, detuned and slowly wobbling: the
    "something is in here with you" layer. Loops seamlessly."""
    dur = 30.0
    ts = t(dur)
    voices = np.zeros_like(ts)
    for f in (146.8, 155.6, 207.7, 311.1):       # D3, D#3, G#3, D#4
        detune = 1 + 0.0015 * np.sin(2 * np.pi * rng.uniform(0.03, 0.09) * ts + rng.uniform(0, 6))
        voices += np.sin(2 * np.pi * f * detune * ts) / 4
    air = bandpass(noise(dur), 1200, 5000) * 0.08
    swell = 0.55 + 0.45 * np.sin(2 * np.pi * 0.023 * ts + 1.1)
    return {"SW_Tension_Cluster_Loop": norm(loopable(lowpass(voices + air, 3500) * swell), -12.0)}


def gen_riser() -> dict[str, np.ndarray]:
    """Shepard tone: octave-spaced partials sliding up while a fixed bell curve
    fades the top ones out. It rises for 12 seconds and never arrives -- this is
    the dizziness trick, straight out of RE and Outlast."""
    dur = 12.0
    ts = t(dur)
    out = np.zeros_like(ts)
    octaves = 7
    for k in range(octaves):
        phase = (ts / dur + k / octaves) % 1.0
        freq = 32.0 * 2 ** (phase * octaves)
        amp = np.exp(-0.5 * ((np.log2(freq) - np.log2(600)) / 1.6) ** 2)
        out += np.sin(2 * np.pi * np.cumsum(freq) / RATE) * amp
    swell = np.linspace(0.25, 1.0, len(ts)) ** 1.6
    return {"SW_Tension_Riser_Shepard": norm(fade(out * swell, 400, 250), -6.0)}


def gen_stinger() -> dict[str, np.ndarray]:
    """The hit. Sub boom for the chest, inharmonic metal for the fear, noise
    slam for the transient. Three variants so repeats are not identical."""
    out = {}
    for i, (boom_f, screech_f, length) in enumerate(
            [(48, 1850, 3.4), (41, 2350, 4.0), (55, 1450, 3.0)], start=1):
        ts = t(length)
        boom = np.sin(2 * np.pi * np.cumsum(np.linspace(boom_f, boom_f * 0.55, len(ts))) / RATE)
        boom *= np.exp(-ts * 1.5)
        metal = np.zeros_like(ts)
        for ratio in (1.0, 1.41, 2.07, 2.73, 3.62):     # inharmonic = unpleasant
            metal += np.sin(2 * np.pi * screech_f * ratio * ts + rng.uniform(0, 6)) / 5
        metal *= np.exp(-ts * 3.0) * 0.5
        slam = highpass(noise(length), 300) * np.exp(-ts * 26) * 0.7
        hit = lowpass(boom * 1.1 + metal + slam, 12000)
        out[f"SW_Tension_Stinger_{i:02d}"] = norm(fade(hit, 1, 300), -2.0)
    return out


def gen_bodythud() -> dict[str, np.ndarray]:
    """Layered under a recorded landing: the weight, not the shoe."""
    out = {}
    for i, f in enumerate((62, 54, 70), start=1):
        ts = t(0.9)
        body = np.sin(2 * np.pi * np.cumsum(np.linspace(f, f * 0.45, len(ts))) / RATE)
        body *= np.exp(-ts * 8.0)
        knock = lowpass(noise(0.9), 400) * np.exp(-ts * 30) * 0.5
        out[f"SW_Foley_BodyThud_{i:02d}"] = norm(fade(lowpass(body + knock, 500), 1, 120), -5.0)
    return out


def gen_arc() -> dict[str, np.ndarray]:
    """Electrical crackle for the failing fixture: random bursts of filtered
    noise with a mains buzz underneath."""
    out = {}
    for i in range(1, 4):
        dur = 2.2
        ts = t(dur)
        crackle = np.zeros_like(ts)
        for _ in range(rng.integers(7, 16)):
            at = rng.uniform(0, dur - 0.2)
            idx = int(at * RATE)
            # Build the burst from a sample count, never from a duration: rounding
            # seconds back to samples can land one sample off its own envelope.
            burst_len = int(RATE * rng.uniform(0.004, 0.05))
            burst = bandpass(rng.normal(0.0, 1.0, burst_len), 1800, 11000)
            burst *= np.exp(-np.linspace(0, 8, burst_len)) * rng.uniform(0.3, 1.0)
            fits = min(burst_len, len(crackle) - idx)
            crackle[idx:idx + fits] += burst[:fits]
        buzz = np.sin(2 * np.pi * 100 * ts) * 0.12 + np.sin(2 * np.pi * 300 * ts) * 0.06
        buzz *= (0.4 + 0.6 * (crackle != 0))
        out[f"SW_Amb_ElecArc_{i:02d}"] = norm(fade(crackle + buzz, 2, 80), -8.0)
    return out


GENERATORS = {
    "heartbeat": gen_heartbeat,
    "tinnitus": gen_tinnitus,
    "subdrone": gen_subdrone,
    "cluster": gen_cluster,
    "riser": gen_riser,
    "stinger": gen_stinger,
    "bodythud": gen_bodythud,
    "arc": gen_arc,
}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("which", nargs="*", default=["all"], help="generator names, or 'all'")
    p.add_argument("-o", "--out", default=".", help="output folder")
    p.add_argument("--seed", type=int, default=7)
    p.add_argument("--list", action="store_true", help="list generators and exit")
    args = p.parse_args()

    if args.list:
        for name, fn in GENERATORS.items():
            print(f"  {name:10s} {(fn.__doc__ or '').strip().splitlines()[0]}")
        return 0

    global rng
    rng = np.random.default_rng(args.seed)

    names = list(GENERATORS) if "all" in args.which else args.which
    unknown = [n for n in names if n not in GENERATORS]
    if unknown:
        print(f"unknown generator(s): {', '.join(unknown)}")
        return 1

    os.makedirs(args.out, exist_ok=True)
    for name in names:
        for filename, audio in GENERATORS[name]().items():
            path = os.path.join(args.out, filename + ".wav")
            wavfile.write(path, RATE, (np.clip(audio, -1, 1) * 32767).astype(np.int16))
            print(f"{filename+'.wav':38s} {len(audio)/RATE:6.2f}s  peak {20*np.log10(np.abs(audio).max()):5.1f} dBFS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
