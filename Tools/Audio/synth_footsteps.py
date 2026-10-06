r"""SEAWALL -- synthesised concrete footsteps, jumps and landings.

These are PLACEHOLDERS with a purpose: they let the whole footstep engine be
built, heard and tuned today instead of waiting on recordings. Real foley
replaces the files, not the system -- the data assets stay exactly as they are.

A footstep on concrete is three things stacked:
    transient  the heel striking stone     (very short filtered noise burst)
    body       the slab answering back     (low resonance, decays fast)
    grit       sand and dust under the sole (brief high noise tail)
Change the mix of those three and the same code gives you a careful creep,
a walk, a run, a scuff or a landing.

    python synth_footsteps.py -o <out dir>            # all sets
    python synth_footsteps.py -o <dir> --takes 12     # more variations

Needs numpy + scipy:
  C:\Users\Intel\AppData\Local\Programs\Python\Python310\python.exe
"""
from __future__ import annotations

import argparse
import os

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter, sosfilt

RATE = 48000
rng = np.random.default_rng(11)


def noise(n: int) -> np.ndarray:
    return rng.normal(0.0, 1.0, n)


def lowpass(x, hz, order=4):
    b, a = butter(order, min(hz / (RATE / 2), 0.99), btype="low")
    return lfilter(b, a, x)


def highpass(x, hz, order=4):
    b, a = butter(order, max(hz / (RATE / 2), 1e-4), btype="high")
    return lfilter(b, a, x)


def bandpass(x, lo, hi, order=4):
    sos = butter(order, [max(lo / (RATE / 2), 1e-4), min(hi / (RATE / 2), 0.99)],
                 btype="band", output="sos")
    return sosfilt(sos, x)


def env(n: int, decay: float) -> np.ndarray:
    return np.exp(-np.linspace(0.0, decay, n))


def norm(x, peak_dbfs):
    p = np.abs(x).max()
    return x * ((10 ** (peak_dbfs / 20)) / p) if p > 0 else x


def step(transient_db, body_hz, body_level, grit_level, length_s, decay, peak_dbfs):
    """One footstep. Randomised a little on every call so no two takes match."""
    n = int(RATE * length_s)
    t = np.linspace(0.0, length_s, n, endpoint=False)

    # Heel strike: a click, not a thump. Short and bright.
    transient = highpass(noise(n), 900) * env(n, decay * 3.2) * (10 ** (transient_db / 20))

    # The slab: two close low resonances, slightly detuned per take.
    f0 = body_hz * rng.uniform(0.94, 1.06)
    body = (np.sin(2 * np.pi * f0 * t) + 0.6 * np.sin(2 * np.pi * f0 * 1.47 * t))
    body *= env(n, decay * 1.6) * body_level

    # Grit under the sole, arriving a hair after the strike like real dust does.
    grit = bandpass(noise(n), 2200, 9000) * env(n, decay * 2.0) * grit_level
    delay = int(RATE * rng.uniform(0.004, 0.012))
    grit = np.concatenate([np.zeros(delay), grit])[:n]

    out = lowpass(transient + body + grit, 15000)
    # Tiny fade in so there is never a click from sample zero.
    fade = int(RATE * 0.0008)
    out[:fade] *= np.linspace(0, 1, fade)
    return norm(out, peak_dbfs + rng.uniform(-1.5, 1.5))


# name -> keyword args for step()
SETS = {
    # Careful creep: almost no transient, no grit, very short.
    "Step_Concrete_Crouch": dict(transient_db=-16, body_hz=110, body_level=0.25,
                                 grit_level=0.05, length_s=0.16, decay=9.0, peak_dbfs=-13),
    # Normal walk: balanced.
    "Step_Concrete_Walk":   dict(transient_db=-7, body_hz=96, body_level=0.45,
                                 grit_level=0.18, length_s=0.26, decay=7.0, peak_dbfs=-7),
    # Run: heavier heel, more body, more grit thrown up.
    "Step_Concrete_Sprint": dict(transient_db=-3, body_hz=82, body_level=0.75,
                                 grit_level=0.32, length_s=0.34, decay=5.5, peak_dbfs=-4),
    # Push-off for a jump: body without much heel click.
    "Step_Concrete_Jump":   dict(transient_db=-9, body_hz=78, body_level=0.8,
                                 grit_level=0.3, length_s=0.3, decay=6.0, peak_dbfs=-6),
    # Landing, soft: both feet, a little heavier than a walk step.
    "Step_Concrete_LandSoft": dict(transient_db=-5, body_hz=74, body_level=0.9,
                                   grit_level=0.28, length_s=0.4, decay=5.0, peak_dbfs=-5),
    # Landing, hard: full weight. The synth body thud layers under this.
    "Step_Concrete_LandHard": dict(transient_db=-1, body_hz=62, body_level=1.3,
                                   grit_level=0.45, length_s=0.6, decay=3.6, peak_dbfs=-2),
}


def scuff(length_s=0.42, peak_dbfs=-9):
    """A drag, not an impact: noise swelling then dying, no transient."""
    n = int(RATE * length_s)
    shape = np.sin(np.linspace(0, np.pi, n)) ** 1.6
    body = bandpass(noise(n), 700, 6500) * shape
    low = lowpass(noise(n), 300) * shape * 0.5
    wobble = 1 + 0.25 * np.sin(2 * np.pi * rng.uniform(7, 16) * np.linspace(0, length_s, n))
    return norm(lowpass(body * wobble + low, 12000), peak_dbfs + rng.uniform(-1.5, 1.5))


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-o", "--out", default=".")
    p.add_argument("--takes", type=int, default=10, help="variations per set")
    p.add_argument("--seed", type=int, default=11)
    args = p.parse_args()

    global rng
    rng = np.random.default_rng(args.seed)
    os.makedirs(args.out, exist_ok=True)

    written = 0
    for name in SETS:
        for i in range(1, args.takes + 1):
            audio = step(**SETS[name])
            path = os.path.join(args.out, f"SW_{name}_{i:02d}.wav")
            wavfile.write(path, RATE, (np.clip(audio, -1, 1) * 32767).astype(np.int16))
            written += 1
        print(f"{name:26s} {args.takes} takes")

    for i in range(1, args.takes + 1):
        audio = scuff()
        wavfile.write(os.path.join(args.out, f"SW_Step_Concrete_Scuff_{i:02d}.wav"),
                      RATE, (np.clip(audio, -1, 1) * 32767).astype(np.int16))
        written += 1
    print(f"{'Step_Concrete_Scuff':26s} {args.takes} takes")
    print(f"\n{written} files -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
