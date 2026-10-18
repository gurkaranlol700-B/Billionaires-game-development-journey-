r"""SEAWALL -- in-game footsteps. The approved K4 design.

HOW THIS ARRIVED HERE (three rounds, two rejected)
  v1  the body of the step was a sine wave with a decay envelope, which is how a
      drum machine makes a tom. It had a pitch, so it sounded like a snare.
  v2  noise instead of tone, but the low end decayed over 70-150 ms with five
      reflections and a long tail. A low frequency that rings on IS a hollow
      box, so it sounded like a wooden plank.
  v3  built like a kick drum, auditioned as six candidates. K4 approved:

K4, THE APPROVED DESIGN
  ONE impact, never two. The second ("toe") hit was what read as a spike at the
  end of every step, so there is none.
  NARROW-BAND NOISE, not a tone. Centred near 95 Hz with a bandwidth wide
  enough that no note can be picked out: the weight of a kick with nothing to
  hum along to.
  A FLAT HOLD ENVELOPE. Fast attack, a level plateau, then a quick release, so
  the sound is as bassy at its end as at its beginning. An exponential decay
  starts loud and collapses, and that collapse is what reads as a spike.
  NO TAIL, NO REFLECTIONS. Those made it hollow.

    python synth_footsteps.py -o <out dir> --takes 10

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
rng = np.random.default_rng(31)


# --- building blocks -------------------------------------------------------

def noise(n):
    return rng.normal(0.0, 1.0, int(n))


def lowpass(x, hz, order=4):
    b, a = butter(order, min(hz / (RATE / 2), 0.99), btype="low")
    return lfilter(b, a, x)


def highpass(x, hz, order=2):
    b, a = butter(order, max(hz / (RATE / 2), 1e-4), btype="high")
    return lfilter(b, a, x)


def bandpass(x, lo, hi, order=4):
    sos = butter(order, [max(lo / (RATE / 2), 1e-4), min(hi / (RATE / 2), 0.99)],
                 btype="band", output="sos")
    return sosfilt(sos, x)


def hold_env(n, attack_ms, hold_ms, release_ms):
    """Fast attack, FLAT plateau, quick release -- the heart of the K4 design.

    The plateau is why the step stays bassy from beginning to end instead of
    spiking and collapsing.
    """
    n = int(n)
    env = np.zeros(n)
    a = min(max(2, int(RATE * attack_ms / 1000.0)), n)
    h = int(RATE * hold_ms / 1000.0)
    r = max(2, int(RATE * release_ms / 1000.0))
    env[:a] = np.linspace(0.0, 1.0, a)
    end_hold = min(n, a + h)
    env[a:end_hold] = 1.0
    if end_hold < n:
        tail = min(r, n - end_hold)
        # Slightly curved: a straight line clicks, an exponential sags.
        env[end_hold:end_hold + tail] = np.linspace(1.0, 0.0, tail) ** 1.4
    return env


def exp_env(n, ms):
    n = int(n)
    t = np.linspace(0.0, 1.0, n)
    env = np.exp(-t * (1000.0 / max(ms, 1.0)) * 0.9)
    a = max(2, int(RATE * 0.0008))
    env[:a] *= np.linspace(0, 1, a)
    return env


def finish(x, peak_dbfs, jitter=True):
    # Nothing below 38 Hz: no speaker reproduces it, and it only eats level.
    x = highpass(x, 38.0, order=2)
    # Soft clip for density without extra level.
    x = np.tanh(x * 1.5) / 1.5
    peak = np.abs(x).max()
    if peak > 0:
        target = peak_dbfs + (rng.uniform(-0.8, 0.8) if jitter else 0.0)
        x = x * ((10 ** (target / 20)) / peak)
    fade = max(2, int(RATE * 0.0005))
    x[:fade] *= np.linspace(0, 1, fade)
    out = max(2, int(RATE * 0.008))
    x[-out:] *= np.linspace(1, 0, out)
    return x


def step(length_ms, centre_hz, width, attack_ms, hold_ms, release_ms,
         click_level, click_hz, click_ms, mid_level, peak_dbfs):
    """One footfall: body, contact click, and a touch of midrange."""
    n = int(RATE * length_ms / 1000.0)
    out = np.zeros(n)

    centre = centre_hz * rng.uniform(0.97, 1.03)
    env = hold_env(n, attack_ms, hold_ms, release_ms)

    # The body. Narrow-band noise: kick weight, no note.
    out += bandpass(noise(n), centre / width, centre * width) * env * 3.4

    # The contact. Short enough to be an attack rather than a tick.
    if click_level > 0:
        out += highpass(noise(n), click_hz) * exp_env(n, click_ms) * click_level

    # A little 220-520 Hz so the step still exists on laptop speakers, which
    # reproduce almost nothing below 150 Hz.
    if mid_level > 0:
        out += bandpass(noise(n), 220.0, 520.0) * hold_env(n, attack_ms, hold_ms * 0.7,
                                                           release_ms * 0.8) * mid_level
    return finish(out, peak_dbfs)


def scuff(peak_dbfs=-12):
    """A drag rather than an impact: no attack, dark, short."""
    length_ms = rng.uniform(260.0, 360.0)
    n = int(RATE * length_ms / 1000.0)
    shape = np.sin(np.linspace(0.0, np.pi, n)) ** 1.8
    body = bandpass(noise(n), 320.0, 2600.0) * shape
    low = lowpass(noise(n), 220.0) * shape * 0.8
    rough = 1.0 + 0.25 * np.sin(2 * np.pi * rng.uniform(7.0, 14.0)
                                * np.linspace(0, length_ms / 1000.0, n))
    return finish(body * rough + low, peak_dbfs)


# K4, scaled per movement. Slowed slightly from the audition: the hold is
# longer, which makes each step read as heavier and less hurried.
SETS = {
    "Step_Concrete_Crouch":   dict(length_ms=185, centre_hz=102, width=1.28, attack_ms=1.2,
                                   hold_ms=52, release_ms=24, click_level=0.10, click_hz=2400,
                                   click_ms=5, mid_level=0.08, peak_dbfs=-15),
    "Step_Concrete_Walk":     dict(length_ms=250, centre_hz=95, width=1.26, attack_ms=0.8,
                                   hold_ms=88, release_ms=28, click_level=0.20, click_hz=2200,
                                   click_ms=7, mid_level=0.13, peak_dbfs=-8),
    "Step_Concrete_Sprint":   dict(length_ms=270, centre_hz=90, width=1.28, attack_ms=0.8,
                                   hold_ms=96, release_ms=32, click_level=0.30, click_hz=2400,
                                   click_ms=8, mid_level=0.18, peak_dbfs=-5),
    "Step_Concrete_Jump":     dict(length_ms=265, centre_hz=92, width=1.26, attack_ms=1.0,
                                   hold_ms=92, release_ms=34, click_level=0.22, click_hz=2000,
                                   click_ms=8, mid_level=0.15, peak_dbfs=-6),
    "Step_Concrete_LandSoft": dict(length_ms=285, centre_hz=88, width=1.26, attack_ms=0.8,
                                   hold_ms=98, release_ms=36, click_level=0.26, click_hz=2200,
                                   click_ms=8, mid_level=0.16, peak_dbfs=-5),
    "Step_Concrete_LandHard": dict(length_ms=340, centre_hz=82, width=1.24, attack_ms=0.6,
                                   hold_ms=118, release_ms=46, click_level=0.38, click_hz=2000,
                                   click_ms=10, mid_level=0.20, peak_dbfs=-3),
}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-o", "--out", default=".")
    p.add_argument("--takes", type=int, default=10)
    p.add_argument("--seed", type=int, default=31)
    args = p.parse_args()

    global rng
    rng = np.random.default_rng(args.seed)
    os.makedirs(args.out, exist_ok=True)

    written = 0
    for name in SETS:
        for i in range(1, args.takes + 1):
            audio = step(**SETS[name])
            wavfile.write(os.path.join(args.out, "SW_%s_%02d.wav" % (name, i)),
                          RATE, (np.clip(audio, -1, 1) * 32767).astype(np.int16))
            written += 1
        print("%-26s %2d takes  centre %3d Hz  hold %3d ms" % (
            name, args.takes, SETS[name]["centre_hz"], SETS[name]["hold_ms"]))

    for i in range(1, args.takes + 1):
        audio = scuff()
        wavfile.write(os.path.join(args.out, "SW_Step_Concrete_Scuff_%02d.wav" % i),
                      RATE, (np.clip(audio, -1, 1) * 32767).astype(np.int16))
        written += 1
    print("%-26s %2d takes" % ("Step_Concrete_Scuff", args.takes))
    print("\n%d files -> %s" % (written, args.out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
