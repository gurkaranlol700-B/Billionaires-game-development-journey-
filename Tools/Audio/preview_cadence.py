r"""SEAWALL -- preview the in-game footstep rhythm outside the game.

The cadence is not a number in the audio; it falls out of the movement code:
steps fire every time the player covers one stride, and stride grows with
speed. This reproduces that arithmetic exactly so the pace can be judged
before compiling anything.

    python preview_cadence.py
"""
import os
import sys

import numpy as np
from scipy.io import wavfile

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import synth_footsteps as sf

RATE = sf.RATE
OUT = r"D:\UnrealProjects\Seawall\Art\Audio\Auditions"

# Straight from SWCharacter.
WALK_SPEED, CROUCH_SPEED, SPRINT_SPEED = 150.0, 75.0, 420.0
STEP_LENGTH, STEP_LENGTH_SPRINT = 82.0, 150.0


def stride_for(speed):
    """The same mapping the character uses: stride grows with speed."""
    lo_speed, hi_speed = CROUCH_SPEED, SPRINT_SPEED
    lo_stride, hi_stride = STEP_LENGTH * 0.85, STEP_LENGTH_SPRINT
    alpha = min(max((speed - lo_speed) / (hi_speed - lo_speed), 0.0), 1.0)
    return lo_stride + alpha * (hi_stride - lo_stride)


def sequence(steps, interval, pad=0.6):
    total = int(RATE * (interval * len(steps) + pad))
    out = np.zeros(total)
    at = 0.05
    for i, s in enumerate(steps):
        gain = 1.0 if i % 2 == 0 else 0.9
        start = int(RATE * at)
        fits = min(len(s), total - start)
        out[start:start + fits] += s[:fits] * gain
        at += interval * sf.rng.uniform(0.96, 1.04)
    peak = np.abs(out).max()
    return out * (0.89 / peak) if peak > 0 else out


MODES = [
    ("Crouch", "Step_Concrete_Crouch", CROUCH_SPEED, 9),
    ("Walk", "Step_Concrete_Walk", WALK_SPEED, 11),
    ("Sprint", "Step_Concrete_Sprint", SPRINT_SPEED, 13),
]

CARD = """<div class="card"><p class="name">%s</p>
<p class="why">%.0f cm/s over a %.0f cm stride = <b>%.2f steps per second</b>, one every %.0f ms.
Previously %s.</p>
<div class="row"><div class="clip"><audio controls preload="none" src="%s"></audio></div></div></div>"""

PREVIOUS = {"Crouch": "every 880 ms", "Walk": "every 440 ms", "Sprint": "every 157 ms (the machine-gun bug)"}


def main():
    os.makedirs(OUT, exist_ok=True)
    cards = []
    for label, set_name, speed, count in MODES:
        stride = stride_for(speed)
        interval = stride / speed
        steps = [sf.step(**sf.SETS[set_name]) for _ in range(count)]
        name = "SW_Preview_K4_%s.wav" % label
        wavfile.write(os.path.join(OUT, name), RATE,
                      (np.clip(sequence(steps, interval), -1, 1) * 32767).astype(np.int16))
        cards.append(CARD % (label, speed, stride, 1.0 / interval, interval * 1000,
                             PREVIOUS[label], name))
        print("%-7s stride %5.1f cm  %.2f steps/s  every %4.0f ms" % (label, stride, 1 / interval, interval * 1000))

    page = """<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SEAWALL K4 cadence preview</title>
<style>
 :root { color-scheme: dark; }
 body { margin:0; padding:28px 20px 60px; background:#0e0f12; color:#e8e6e3;
        font:15px/1.6 "Segoe UI", system-ui, sans-serif; }
 .wrap { max-width:760px; margin:0 auto; }
 h1 { font-size:25px; margin:0 0 6px; }
 .sub { color:#9a978f; margin:0 0 24px; }
 .card { border:1px solid #262a31; border-radius:10px; padding:16px 18px; margin:0 0 14px;
         background:#14161a; }
 .name { font-weight:650; font-size:17px; margin:0 0 4px; }
 .why { color:#b9b5ad; margin:0 0 12px; }
 .why b { color:#e8e6e3; }
 .row { display:flex; gap:14px; }
 .clip { flex:1 1 auto; }
 audio { width:100%; }
 .note { border-left:3px solid #3b4654; padding:10px 14px; margin:22px 0; background:#121418;
         color:#b9b5ad; }
 .note b { color:#e8e6e3; }
</style>
<div class="wrap">
<h1>K4, slowed, at the real cadence</h1>
<p class="sub">The approved kick design with a longer hold, played at the exact rhythm the movement
code now produces. If the pace is right, this goes into the game.</p>
__CARDS__
<div class="note">
<p><b>What changed in the sound.</b> Same K4: one impact, narrow-band noise near 95 Hz, flat hold so
the bass does not collapse into a spike. The hold went from 80 to 88 ms and the step is longer
overall, which makes each one read as heavier and less hurried.</p>
<p><b>What changed in the movement.</b> Stride went from 66 to 82 cm, and sprinting now uses a 150 cm
stride instead of the same short one, so running is 2.8 steps a second instead of 6.4. The head bob
is now drawn from the very same stride counter the footstep fires on, with its lowest point landing
just after each footfall -- so the camera and the feet are synchronised by construction, not by
being tuned to match. That part can only be judged in the game.</p>
</div>
</div>
""".replace("__CARDS__", chr(10).join(cards))

    path = os.path.join(OUT, "k4-cadence-preview.html")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(page)
    print("\npage: %s" % path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
