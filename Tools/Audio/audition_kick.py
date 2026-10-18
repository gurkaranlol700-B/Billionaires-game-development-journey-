r"""SEAWALL -- footsteps built like a kick drum. Round 3 of the audition.

FEEDBACK THIS ROUND ANSWERS
  "still feels pitchy with a spike at the end"  -> the spike was the TOE: a
      second impact 26-55 ms after the heel, with the reflection tail arriving
      behind it. Every candidate here is a SINGLE impact, no toe, no tail.
  "bassy from the beginning until the end and stays the same" -> an exponential
      decay starts loud and collapses, which is what reads as a spike. These
      use a flat HOLD envelope: fast attack, level plateau, quick release.
  "like a kick drum / foot kick"  -> so the body is built like one.

THE PITCH PROBLEM WITH KICKS
A real kick drum is a tuned membrane: its body is a sine sweeping downward,
which is a NOTE. That is the pitchiness that keeps coming back. So most of
these build the body from narrow-band NOISE centred around 90-130 Hz instead
-- the same weight and punch, with no note to hear. Candidate K3 is a genuine
swept-sine kick, included so the difference is audible rather than theoretical.

    python audition_kick.py

Needs numpy + scipy:
  C:\Users\Intel\AppData\Local\Programs\Python\Python310\python.exe
"""
from __future__ import annotations

import argparse
import glob
import os

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter, sosfilt

RATE = 48000
rng = np.random.default_rng(31)

WALK_INTERVAL = 0.44
SPRINT_INTERVAL = 0.31


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
    """Fast attack, FLAT plateau, quick release.

    This is the shape being asked for: the sound is as bassy at the end as at
    the start, so there is no spike. An exponential decay cannot do this.
    """
    n = int(n)
    env = np.zeros(n)
    a = max(2, int(RATE * attack_ms / 1000.0))
    h = int(RATE * hold_ms / 1000.0)
    r = max(2, int(RATE * release_ms / 1000.0))
    a = min(a, n)
    env[:a] = np.linspace(0.0, 1.0, a)
    end_hold = min(n, a + h)
    env[a:end_hold] = 1.0
    if end_hold < n:
        tail = min(r, n - end_hold)
        # Slightly curved release: straight lines click, exponentials sag.
        env[end_hold:end_hold + tail] = np.linspace(1.0, 0.0, tail) ** 1.4
    return env


def exp_env(n, ms):
    n = int(n)
    t = np.linspace(0.0, 1.0, n)
    env = np.exp(-t * (1000.0 / max(ms, 1.0)) * 0.9)
    a = max(2, int(RATE * 0.0008))
    env[:a] *= np.linspace(0, 1, a)
    return env


def finish(x, peak_dbfs, hpf=38.0):
    x = highpass(x, hpf, order=2)
    # Soft clip: adds density and makes it feel solid without raising the peak.
    x = np.tanh(x * 1.5) / 1.5
    peak = np.abs(x).max()
    if peak > 0:
        x = x * ((10 ** (peak_dbfs / 20)) / peak)
    fade = max(2, int(RATE * 0.0005))
    x[:fade] *= np.linspace(0, 1, fade)
    out = max(2, int(RATE * 0.008))
    x[-out:] *= np.linspace(1, 0, out)
    return x


def kick_step(length_ms, body_mode, centre_hz, width, attack_ms, hold_ms, release_ms,
              click_level, click_hz, click_ms, mid_level, mid_lo, mid_hi, sweep_to,
              peak_dbfs, jitter=True):
    n = int(RATE * length_ms / 1000.0)
    t = np.linspace(0.0, length_ms / 1000.0, n, endpoint=False)
    out = np.zeros(n)

    wobble = rng.uniform(0.96, 1.04) if jitter else 1.0
    centre = centre_hz * wobble
    env = hold_env(n, attack_ms, hold_ms, release_ms)

    if body_mode in ("noise", "both"):
        # Narrow-band noise: the weight of a kick with no note in it.
        lo = centre / width
        hi = centre * width
        out += bandpass(noise(n), lo, hi, order=4) * env * 3.4

    if body_mode in ("sine", "both"):
        # A real kick: pitch sweeping down. Punchy, and audibly a note.
        freq = np.linspace(centre, sweep_to * wobble, n)
        out += np.sin(2 * np.pi * np.cumsum(freq) / RATE) * env * 0.9

    # The contact click. Short enough to be an attack, not a tick.
    if click_level > 0:
        click = highpass(noise(n), click_hz) * exp_env(n, click_ms) * click_level
        out += click

    # A little 200-500 Hz so it is still there on laptop speakers, which
    # reproduce almost nothing below 150 Hz.
    if mid_level > 0:
        out += bandpass(noise(n), mid_lo, mid_hi) * hold_env(n, attack_ms, hold_ms * 0.7,
                                                             release_ms * 0.8) * mid_level

    return finish(out, peak_dbfs + (rng.uniform(-0.8, 0.8) if jitter else 0.0))


def base(**over):
    p = dict(length_ms=190, body_mode="noise", centre_hz=110.0, width=1.35, attack_ms=1.5,
             hold_ms=55.0, release_ms=45.0, click_level=0.25, click_hz=2200.0, click_ms=6.0,
             mid_level=0.18, mid_lo=220.0, mid_hi=520.0, sweep_to=55.0, peak_dbfs=-7.0)
    p.update(over)
    return p


CANDIDATES = [
    ("K1", "Kick, pure thump",
     "Narrow-band noise at 110 Hz with a flat hold: bassy from the first millisecond to the last, no decay, no spike, barely any click. The plainest version of what you described.",
     base()),

    ("K2", "Kick plus tile click",
     "Same thump, with a brighter and slightly longer contact click on top so it still reads as a shoe hitting a hard floor rather than a drum alone.",
     base(click_level=0.55, click_hz=2600, click_ms=9, mid_level=0.26)),

    ("K3", "Real swept kick",
     "An actual kick drum body: a sine sweeping 130 Hz down to 55. The punchiest of the set and the reference for what pitch sounds like, so you can tell me if the note bothers you or not.",
     base(body_mode="sine", centre_hz=130, sweep_to=55, hold_ms=45, release_ms=55, click_level=0.3)),

    ("K4", "Flat top, square hold",
     "The most extreme reading of stays the same: near-instant attack, 80 ms of completely level bass, then it stops. Unnaturally even, which may be exactly the effect you want.",
     base(length_ms=220, attack_ms=0.8, hold_ms=80, release_ms=22, centre_hz=100,
          width=1.25, click_level=0.18, mid_level=0.12)),

    ("K5", "Quick and higher",
     "Shorter and tuned up to 135 Hz, so it is a tight bassy knock rather than a boom. For when the others feel too low or too slow.",
     base(length_ms=150, centre_hz=135, width=1.3, hold_ms=38, release_ms=30,
          click_level=0.35, mid_level=0.22)),

    ("K6", "Kick with a sub under it",
     "Noise kick at 115 Hz plus a swept sine underneath for extra chest, both cut off together so nothing rings on. Heaviest of the set while still being quick.",
     base(body_mode="both", centre_hz=115, sweep_to=62, hold_ms=60, release_ms=40,
          click_level=0.3, mid_level=0.2, peak_dbfs=-6)),
]


def sequence(steps, interval, pad_end=0.5):
    total = int(RATE * (interval * len(steps) + pad_end))
    out = np.zeros(total)
    at = 0.05
    for i, s in enumerate(steps):
        gain = 1.0 if i % 2 == 0 else 0.9
        start = int(RATE * at)
        fits = min(len(s), total - start)
        out[start:start + fits] += s[:fits] * gain
        at += interval * rng.uniform(0.95, 1.05)
    peak = np.abs(out).max()
    if peak > 0:
        out = out * (0.89 / peak)
    return out


def write(path, audio):
    wavfile.write(path, RATE, (np.clip(audio, -1, 1) * 32767).astype(np.int16))


def load_walk_loop(pattern, takes=8):
    files = sorted(glob.glob(pattern))[:takes]
    out = []
    for path in files:
        _rate, data = wavfile.read(path)
        out.append(data.astype(np.float64) / 32768.0)
    return out


PAGE_HEAD = """<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SEAWALL footsteps - kick round</title>
<style>
  :root { color-scheme: dark; }
  body { margin:0; padding:28px 20px 60px; background:#0e0f12; color:#e8e6e3;
         font:15px/1.6 "Segoe UI", system-ui, sans-serif; }
  .wrap { max-width:860px; margin:0 auto; }
  h1 { font-size:26px; margin:0 0 6px; }
  .sub { color:#9a978f; margin:0 0 24px; }
  .card { border:1px solid #262a31; border-radius:10px; padding:16px 18px; margin:0 0 14px;
          background:#14161a; }
  .card.ref { border-color:#4a4030; background:#17150f; }
  .name { font-weight:650; font-size:17px; margin:0 0 2px; }
  .id { color:#7d8894; font-family:ui-monospace, Consolas, monospace; font-size:12px; }
  .why { color:#b9b5ad; margin:8px 0 12px; }
  .row { display:flex; flex-wrap:wrap; gap:14px; }
  .clip { flex:1 1 240px; min-width:220px; }
  .clip span { display:block; color:#8d99a6; font-size:12px; text-transform:uppercase;
               letter-spacing:.06em; margin-bottom:4px; }
  audio { width:100%; }
  .note { border-left:3px solid #3b4654; padding:10px 14px; margin:22px 0; background:#121418;
          color:#b9b5ad; }
  .note b { color:#e8e6e3; }
</style>
<div class="wrap">
<h1>Footsteps, built like a kick drum</h1>
<p class="sub">Single impact each &mdash; no toe, no tail, so nothing spikes at the end. Flat hold
envelopes, so the bass is there from the first millisecond to the last. Listen to the walking loops
first, then tell me a code, or a code plus a change.</p>
"""

PAGE_NOTE = """<div class="note">
<p><b>Why these are built from noise, not from a tone.</b> A real kick drum is a tuned membrane: its
body is a sine sweeping downward, which is a note &mdash; and that note is the pitchiness you keep
hearing. Most of these use narrow-band noise around 100&ndash;135&nbsp;Hz instead, which has the
weight and the punch with nothing to hum along to. <b>K3</b> is a genuine swept-sine kick, included
on purpose: if K3 sounds right to you, the pitch was never the problem and I will stop avoiding it.</p>
<p><b>Still outstanding, whichever you pick.</b> Sprinting currently fires about 6.4 steps a second
because steps are triggered by distance with one fixed stride. These sprint loops use a realistic
rate; the code fix goes in with the approved sound.</p>
</div>
"""


def clip(label, rel):
    return ('<div class="clip"><span>%s</span><audio controls preload="none" src="%s"></audio></div>'
            % (label, rel))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-o", "--out", default=r"D:\UnrealProjects\Seawall\Art\Audio\Auditions")
    p.add_argument("--seed", type=int, default=31)
    args = p.parse_args()

    global rng
    rng = np.random.default_rng(args.seed)
    os.makedirs(args.out, exist_ok=True)

    cards = []
    for key, title, why, params in CANDIDATES:
        walk = [kick_step(**params) for _ in range(8)]
        sprint = [kick_step(**dict(params, peak_dbfs=params["peak_dbfs"] + 1.0)) for _ in range(7)]
        single = kick_step(**dict(params, jitter=False))

        files = {}
        for label, audio in (("WalkLoop", sequence(walk, WALK_INTERVAL)),
                             ("SprintLoop", sequence(sprint, SPRINT_INTERVAL)),
                             ("Single", single)):
            name = "SW_Audition_%s_%s.wav" % (key, label)
            write(os.path.join(args.out, name), audio)
            files[label] = name

        cards.append('<div class="card"><p class="name">%s &mdash; %s</p><p class="why">%s</p>'
                     '<div class="row">%s%s%s</div></div>'
                     % (key, title, why, clip("walking loop", files["WalkLoop"]),
                        clip("sprinting loop", files["SprintLoop"]),
                        clip("one hit", files["Single"])))
        print("rendered %s - %s" % (key, title))

    # Carry the previous round's heaviest option across for comparison.
    previous = os.path.join(args.out, "SW_Audition_E_heavy_tight_WalkLoop.wav")
    if os.path.exists(previous):
        cards.append('<div class="card ref"><p class="name">Previous round, E &mdash; heavy but tight '
                     '<span class="id">for comparison</span></p>'
                     '<p class="why">The best of the foley-style attempts, so you can hear whether the '
                     'kick approach is actually an improvement or just different.</p>'
                     '<div class="row">%s</div></div>'
                     % clip("walking loop", os.path.basename(previous)))

    page = PAGE_HEAD + "\n".join(cards) + PAGE_NOTE + "</div>\n"
    path = os.path.join(args.out, "footstep-kick-audition.html")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(page)
    print("\npage: %s" % path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
