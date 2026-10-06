"""SEAWALL -- cut a long recording into individual takes, ready for Unreal.

A library footstep recording is usually one long file with thirty steps in it.
Unreal wants thirty short files, each starting exactly at the transient, so the
game can pick one at random per step. This does that:

  1. find where each hit starts (a jump in short-term energy above the noise floor)
  2. cut from just before the hit until the tail falls back into the noise
  3. fade the edges so no click is introduced, make mono, resample to 48 kHz
  4. normalise every take to the same peak so the random picks match in level
  5. write SW_<prefix>_01.wav, _02 ... ready to drop into Content/Audio/

    python slice_takes.py <file-or-folder> -o <out dir> --prefix Step_Concrete_Walk

Always run with --dry-run first: it prints what it would cut without writing.

Needs numpy + scipy, so use:
  C:\\Users\\Intel\\AppData\\Local\\Programs\\Python\\Python310\\python.exe
"""
from __future__ import annotations

import argparse
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly

TARGET_RATE = 48000


def to_mono_float(data: np.ndarray, dtype) -> np.ndarray:
    """WAV files arrive as int16/int24-in-int32/float32. Normalise to [-1, 1]."""
    audio = data.astype(np.float64)
    if np.issubdtype(dtype, np.integer):
        audio /= float(np.iinfo(dtype).max + 1)
    if audio.ndim > 1:
        audio = audio.mean(axis=1)
    return audio


def envelope(audio: np.ndarray, rate: int, hop_ms: float = 2.0, win_ms: float = 10.0):
    hop = max(1, int(rate * hop_ms / 1000))
    win = max(hop, int(rate * win_ms / 1000))
    pads = np.pad(np.abs(audio), (0, win))
    frames = np.lib.stride_tricks.sliding_window_view(pads, win)[::hop]
    return np.sqrt((frames ** 2).mean(axis=1)), hop


def find_takes(audio, rate, threshold_db, min_gap_ms, min_len_ms, tail_db):
    """Return (start, end) sample pairs, one per detected hit."""
    env, hop = envelope(audio, rate)
    noise = np.percentile(env, 20) + 1e-9          # the room, not the hits
    peak = env.max() + 1e-9
    open_level = peak * (10 ** (threshold_db / 20))
    close_level = max(noise * 2.0, peak * (10 ** (tail_db / 20)))

    takes, i = [], 0
    min_gap = int(min_gap_ms * rate / 1000)
    min_len = int(min_len_ms * rate / 1000)
    while i < len(env):
        if env[i] < open_level:
            i += 1
            continue
        start = i
        while i < len(env) and env[i] >= close_level:
            i += 1
        end = i
        s, e = start * hop, min(end * hop, len(audio))
        if e - s >= min_len:
            if takes and s - takes[-1][1] < min_gap:   # merge a split transient
                takes[-1] = (takes[-1][0], e)
            else:
                takes.append((s, e))
        i += 1
    return takes


def shape(take: np.ndarray, rate: int, pre_ms: float, fade_in_ms: float, fade_out_ms: float,
          peak_dbfs: float) -> np.ndarray:
    fade_in = max(1, int(rate * fade_in_ms / 1000))
    fade_out = max(1, int(rate * fade_out_ms / 1000))
    out = take.copy()
    if len(out) > fade_in + fade_out:
        out[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
        out[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)
    peak = np.abs(out).max()
    if peak > 0:
        out *= (10 ** (peak_dbfs / 20)) / peak
    return out


def process(path: str, args, counter: list[int]) -> None:
    rate, data = wavfile.read(path)
    audio = to_mono_float(data, data.dtype)

    takes = find_takes(audio, rate, args.threshold_db, args.min_gap_ms, args.min_len_ms, args.tail_db)
    pre = int(rate * args.pre_ms / 1000)
    print(f"\n{os.path.basename(path)}: {len(audio)/rate:.1f}s at {rate} Hz -> {len(takes)} takes")

    for start, end in takes:
        if counter[0] > args.max:
            print("  reached --max, stopping")
            return
        chunk = audio[max(0, start - pre):min(len(audio), end + pre)]
        chunk = shape(chunk, rate, args.pre_ms, args.fade_in_ms, args.fade_out_ms, args.peak_dbfs)
        if rate != TARGET_RATE:
            chunk = resample_poly(chunk, TARGET_RATE, rate)

        name = f"SW_{args.prefix}_{counter[0]:02d}.wav"
        print(f"  {name}  {len(chunk)/TARGET_RATE*1000:6.0f} ms  from {start/rate:7.2f}s")
        if not args.dry_run:
            os.makedirs(args.out, exist_ok=True)
            pcm = np.clip(chunk, -1.0, 1.0)
            wavfile.write(os.path.join(args.out, name), TARGET_RATE, (pcm * 32767).astype(np.int16))
        counter[0] += 1


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("source", help="wav file, or folder of wavs")
    p.add_argument("-o", "--out", default=".", help="output folder")
    p.add_argument("--prefix", default="Take", help="e.g. Step_Concrete_Walk")
    p.add_argument("--dry-run", action="store_true", help="print the cuts, write nothing")
    p.add_argument("--max", type=int, default=24, help="stop after this many takes")
    p.add_argument("--threshold-db", type=float, default=-26.0, help="hit starts this far below the file's peak")
    p.add_argument("--tail-db", type=float, default=-46.0, help="hit ends when it falls this far below peak")
    p.add_argument("--min-gap-ms", type=float, default=90.0, help="closer than this = one hit, not two")
    p.add_argument("--min-len-ms", type=float, default=40.0, help="ignore blips shorter than this")
    p.add_argument("--pre-ms", type=float, default=12.0, help="keep this much before the transient")
    p.add_argument("--fade-in-ms", type=float, default=3.0)
    p.add_argument("--fade-out-ms", type=float, default=25.0)
    p.add_argument("--peak-dbfs", type=float, default=-3.0, help="normalise every take to this peak")
    args = p.parse_args()

    if os.path.isdir(args.source):
        sources = [os.path.join(args.source, f) for f in sorted(os.listdir(args.source))
                   if f.lower().endswith(".wav")]
    else:
        sources = [args.source]
    if not sources:
        print("no wav files found", file=sys.stderr)
        return 1

    counter = [1]
    for src in sources:
        process(src, args, counter)
    print(f"\n{counter[0]-1} takes {'previewed' if args.dry_run else 'written to ' + args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
