r"""SEAWALL -- build footstep candidates and a web page to audition them.

Nothing here touches the game. It renders several complete design directions,
sequences each one into a walking loop at the real in-game cadence, and writes
an HTML page with players so the candidates can be compared by ear. Only the
approved one gets compiled into the project.

Why loops rather than single hits: a footstep is judged in rhythm. One hit in
isolation tells you almost nothing about whether walking feels right.

WHAT WENT WRONG BEFORE (kept here so it does not come back)
  v1  the "body" layer was a sine wave with an envelope -- that is a drum, and
      it had a pitch, so it sounded like a snare
  v2  noise instead of tone, but the low layer decayed over 70-150 ms with five
      reflections and a long tail. A low frequency that rings on IS a hollow
      box, so it sounded like a wooden plank

Hard floors -- tile, concrete -- are the opposite: a dense slap, a thump that
dies in 25-40 ms, and very little tail.

    python audition_footsteps.py -o <out dir>

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
rng = np.random.default_rng(5)

# In-game cadence. Walk: 150 cm/s over a 66 cm stride. Sprint is rendered at a
# realistic rate rather than the current code's 6.4 steps/s, which is a bug
# listed in the page itself.
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


def decay(n, ms):
    n = int(n)
    t = np.linspace(0.0, 1.0, n)
    env = np.exp(-t * (1000.0 / max(ms, 1.0)) * 0.9)
    attack = max(2, int(RATE * 0.0008))
    if n > attack:
        env[:attack] *= np.linspace(0.0, 1.0, attack)
    return env


def place(target, part, at_ms, gain=1.0):
    start = int(RATE * at_ms / 1000.0)
    fits = min(len(part), len(target) - start)
    if fits > 0:
        target[start:start + fits] += part[:fits] * gain


def strike(p, scale=1.0):
    """One contact: slap (the floor), weight (the body), grit (the dust)."""
    n = int(RATE * p["length_ms"] / 1000.0)
    out = np.zeros(n)

    # The slap is the floor itself. On tile and concrete this is the sound.
    out += bandpass(noise(n), p["slap_lo"], p["slap_hi"]) * decay(n, p["slap_ms"]) * p["slap_level"] * scale

    # The weight. Short on a hard floor: a long one is what makes wood hollow.
    out += lowpass(noise(n), p["weight_hz"]) * decay(n, p["weight_ms"]) * p["weight_level"] * 3.0 * scale

    # Grit, a few ms late.
    if p["grit_level"] > 0:
        grit = highpass(noise(n), p["bright_hz"]) * decay(n, p["grit_ms"]) * p["grit_level"] * scale
        delayed = np.zeros(n)
        place(delayed, grit, rng.uniform(2.5, 7.0))
        out += delayed
    return out


def room(x, p):
    """Early reflections and a tail. Few and quiet for a dry hard room."""
    out = x.copy()
    for i in range(p["refl_count"]):
        at = p["refl_spacing_ms"] * (i + 1) * rng.uniform(0.9, 1.1)
        gain = p["refl_wet"] * (0.62 ** i)
        place(out, lowpass(x, p["refl_damp_hz"]), at, gain)
    if p["tail_ms"] > 0:
        n = int(RATE * p["tail_ms"] / 1000.0)
        tail = lowpass(noise(n), p["refl_damp_hz"] * 0.8) * decay(n, p["tail_ms"] * 0.45)
        place(out, tail, 8.0, p["tail_level"] * float(np.abs(x).max()))
    return out


def finish(x, peak_dbfs):
    x = highpass(x, 42.0, order=2)
    x = np.tanh(x * 1.3) / 1.3
    peak = np.abs(x).max()
    if peak > 0:
        x = x * ((10 ** (peak_dbfs / 20)) / peak)
    fade = int(RATE * 0.0006)
    x[:fade] *= np.linspace(0, 1, fade)
    out = int(RATE * 0.015)
    x[-out:] *= np.linspace(1, 0, out)
    return x


def step(p, heavier=1.0, peak_offset=0.0):
    n = int(RATE * p["length_ms"] / 1000.0)
    out = np.zeros(n)
    place(out, strike(p, heavier), 0.0)
    if p["toe_gain"] > 0:
        toe = dict(p)
        toe["length_ms"] = p["length_ms"] * 0.6
        toe["weight_ms"] = p["weight_ms"] * 0.7
        place(out, strike(toe, heavier * 0.8), p["toe_delay_ms"] * rng.uniform(0.85, 1.15), p["toe_gain"])
    return finish(room(out, p), p["peak_dbfs"] + peak_offset + rng.uniform(-1.0, 1.0))


def sequence(steps, interval, pad_end=0.6):
    """Lay steps end to end at a cadence, with human unevenness."""
    total = int(RATE * (interval * len(steps) + pad_end))
    out = np.zeros(total)
    at = 0.05
    for i, s in enumerate(steps):
        # Left and right feet are never identical; alternate slightly.
        gain = 1.0 if i % 2 == 0 else 0.88
        place(out, s, at * 1000.0, gain)
        at += interval * rng.uniform(0.94, 1.06)
    peak = np.abs(out).max()
    if peak > 0:
        out = out * (0.89 / peak)
    return out


def write(path, audio):
    wavfile.write(path, RATE, (np.clip(audio, -1, 1) * 32767).astype(np.int16))


# --- the candidates --------------------------------------------------------
# Each is a complete direction, not a tweak of the last one.

def base(**over):
    p = dict(length_ms=300, slap_lo=300.0, slap_hi=2000.0, slap_ms=24.0, slap_level=0.7,
             weight_hz=130.0, weight_ms=30.0, weight_level=0.8, grit_level=0.2, grit_ms=18.0,
             bright_hz=2800.0, toe_gain=0.3, toe_delay_ms=40.0, refl_count=3,
             refl_spacing_ms=6.0, refl_wet=0.3, refl_damp_hz=2400.0, tail_ms=120.0,
             tail_level=0.08, peak_dbfs=-8.0)
    p.update(over)
    return p


CANDIDATES = [
    ("A_tile_dry", "Tile, dry and tight",
     "Hard tiled floor in a small room. The floor slap is the whole sound, the thump dies in 22 ms, almost no room. Driest and most neutral of the set.",
     base(length_ms=240, slap_lo=350, slap_hi=2600, slap_ms=20, slap_level=0.95,
          weight_hz=150, weight_ms=22, weight_level=0.5, grit_level=0.14, toe_gain=0.22,
          toe_delay_ms=38, refl_count=2, refl_spacing_ms=5.0, refl_wet=0.18,
          refl_damp_hz=3000, tail_ms=60, tail_level=0.05, peak_dbfs=-9)),

    ("B_concrete_natural", "Concrete, natural",
     "A plain shoe on a concrete floor with the corridor just audible behind it. Middle of the road: some weight, some room, nothing exaggerated.",
     base()),

    ("C_boot_stone", "Boot on stone",
     "Heavier shoe, clear heel-then-toe, weight kept short so it stays stone rather than wood. Closest to Resident Evil footsteps.",
     base(length_ms=330, slap_lo=250, slap_hi=1500, slap_ms=30, slap_level=0.6,
          weight_hz=110, weight_ms=38, weight_level=1.0, grit_level=0.26, grit_ms=24,
          toe_gain=0.5, toe_delay_ms=30, refl_count=3, refl_spacing_ms=7.0, refl_wet=0.35,
          refl_damp_hz=2000, tail_ms=150, tail_level=0.1, peak_dbfs=-7)),

    ("D_thin_sole", "Thin sole, bright",
     "A light shoe with a thin sole: bright, snappy, barely any low end. The most tiled of the set, and the least frightening.",
     base(length_ms=200, slap_lo=500, slap_hi=4200, slap_ms=16, slap_level=1.0,
          weight_hz=180, weight_ms=16, weight_level=0.3, grit_level=0.1, grit_ms=12,
          bright_hz=3600, toe_gain=0.18, toe_delay_ms=34, refl_count=2, refl_spacing_ms=4.5,
          refl_wet=0.15, refl_damp_hz=4000, tail_ms=50, tail_level=0.04, peak_dbfs=-10)),

    ("E_heavy_tight", "Heavy, but tight",
     "The weight of the previous version without the hollowness: more mass, far shorter decay, reflections pulled right back.",
     base(length_ms=300, slap_lo=300, slap_hi=1600, slap_ms=22, slap_level=0.65,
          weight_hz=100, weight_ms=28, weight_level=1.3, grit_level=0.22, toe_gain=0.45,
          toe_delay_ms=26, refl_count=2, refl_spacing_ms=5.5, refl_wet=0.22,
          refl_damp_hz=2200, tail_ms=80, tail_level=0.06, peak_dbfs=-7)),

    ("F_damp_corridor", "Damp corridor",
     "Same shoe as B but the room is wetter and darker: more reflections, damped high end, longer tail. The island, not a studio.",
     base(length_ms=330, slap_lo=300, slap_hi=2000, slap_ms=24, slap_level=0.7,
          weight_hz=120, weight_ms=30, weight_level=0.85, grit_level=0.16, toe_gain=0.3,
          toe_delay_ms=40, refl_count=4, refl_spacing_ms=8.0, refl_wet=0.4,
          refl_damp_hz=1600, tail_ms=220, tail_level=0.12, peak_dbfs=-8)),
]


def load_current_walk(takes=8):
    """The version in the game now, for A/B against the candidates."""
    folder = r"D:\UnrealProjects\Seawall\Art\Audio\Source\Generated\Footsteps"
    files = sorted(glob.glob(os.path.join(folder, "SW_Step_Concrete_Walk_*.wav")))[:takes]
    out = []
    for path in files:
        rate, data = wavfile.read(path)
        out.append(data.astype(np.float64) / 32768.0)
    return out


PAGE_HEAD = """<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SEAWALL footstep audition</title>
<style>
  :root { color-scheme: dark; }
  body { margin: 0; padding: 28px 20px 60px; background: #0e0f12; color: #e8e6e3;
         font: 15px/1.6 "Segoe UI", system-ui, sans-serif; }
  .wrap { max-width: 860px; margin: 0 auto; }
  h1 { font-size: 26px; letter-spacing: .02em; margin: 0 0 6px; }
  .sub { color: #9a978f; margin: 0 0 26px; }
  .card { border: 1px solid #262a31; border-radius: 10px; padding: 16px 18px;
          margin: 0 0 14px; background: #14161a; }
  .card.current { border-color: #5a3a3a; background: #171316; }
  .name { font-weight: 650; font-size: 17px; margin: 0 0 2px; }
  .id { color: #7d8894; font-family: ui-monospace, Consolas, monospace; font-size: 12px; }
  .why { color: #b9b5ad; margin: 8px 0 12px; }
  .row { display: flex; flex-wrap: wrap; gap: 14px; }
  .clip { flex: 1 1 250px; min-width: 230px; }
  .clip span { display: block; color: #8d99a6; font-size: 12px;
               text-transform: uppercase; letter-spacing: .06em; margin-bottom: 4px; }
  audio { width: 100%; }
  .note { border-left: 3px solid #3b4654; padding: 10px 14px; margin: 22px 0;
          background: #121418; color: #b9b5ad; }
  .note b { color: #e8e6e3; }
</style>
<div class="wrap">
<h1>Footstep audition</h1>
<p class="sub">Listen to the walking loops first &mdash; a footstep is judged in rhythm, not as one hit.
Pick the letter you want, or say what to change about it. Nothing goes into the game until you approve it.</p>
"""

PAGE_NOTE = """<div class="note">
<p><b>What was wrong before.</b> Version&nbsp;1 built the body of the step from a sine wave with a
decay envelope &mdash; which is how a drum machine makes a tom, so it had a pitch and sounded like a
snare. Version&nbsp;2 replaced the tone with noise but let the low end decay over 70&ndash;150&nbsp;ms
with five reflections and a long tail: a low frequency that rings on <i>is</i> a hollow box, so it
sounded like a wooden plank. Every candidate here keeps the thump between 16 and 38&nbsp;ms and the
room much drier.</p>
<p><b>Also found, not yet fixed.</b> Steps are triggered by distance travelled using one fixed
stride, so sprinting fires about 6.4 steps per second &mdash; machine-gun feet. Real sprinting is
nearer 3. The sprint loops here use the realistic rate; the fix is a stride that grows with speed,
and it goes in with whichever sound you choose.</p>
</div>
"""


def clip_html(label, rel):
    return ('<div class="clip"><span>%s</span><audio controls preload="none" src="%s"></audio></div>'
            % (label, rel))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-o", "--out", default=r"D:\UnrealProjects\Seawall\Art\Audio\Auditions")
    p.add_argument("--seed", type=int, default=5)
    args = p.parse_args()

    global rng
    rng = np.random.default_rng(args.seed)
    os.makedirs(args.out, exist_ok=True)

    cards = []

    # The current in-game sound, for comparison.
    current = load_current_walk()
    if current:
        name = "SW_Audition_Current_WalkLoop.wav"
        write(os.path.join(args.out, name), sequence(current, WALK_INTERVAL))
        cards.append(
            '<div class="card current"><p class="name">The one in the game now '
            '<span class="id">(too heavy / wooden)</span></p>'
            '<p class="why">Here for comparison only.</p>'
            '<div class="row">%s</div></div>' % clip_html("walking loop", name))

    for key, title, why, params in CANDIDATES:
        walk = [step(params) for _ in range(8)]
        sprint = [step(params, heavier=1.25, peak_offset=1.5) for _ in range(7)]
        land = step(params, heavier=2.0, peak_offset=3.0)

        files = {}
        for label, audio in (("WalkLoop", sequence(walk, WALK_INTERVAL)),
                             ("SprintLoop", sequence(sprint, SPRINT_INTERVAL)),
                             ("Landing", land)):
            filename = "SW_Audition_%s_%s.wav" % (key, label)
            write(os.path.join(args.out, filename), audio)
            files[label] = filename

        letter = key.split("_")[0]
        cards.append(
            '<div class="card"><p class="name">%s &mdash; %s <span class="id">%s</span></p>'
            '<p class="why">%s</p><div class="row">%s%s%s</div></div>'
            % (letter, title, key, why,
               clip_html("walking loop", files["WalkLoop"]),
               clip_html("sprinting loop", files["SprintLoop"]),
               clip_html("hard landing", files["Landing"])))
        print("rendered %s - %s" % (letter, title))

    page = PAGE_HEAD + "\n".join(cards) + PAGE_NOTE + "</div>\n"
    page_path = os.path.join(args.out, "footstep-audition.html")
    with open(page_path, "w", encoding="utf-8") as handle:
        handle.write(page)

    print("\npage: %s" % page_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
