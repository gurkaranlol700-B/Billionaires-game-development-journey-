"""SEAWALL -- catalogue a sound library so we can find usable takes fast.

Walks a folder of WAV files, reads each header (sample rate, channels, bit depth,
length) plus the broadcast-WAV description chunk that libraries write their notes
into, sorts every file into the categories this game needs, and writes a CSV.

    python inventory_library.py <library folder> [-o out.csv]

Standard library only, so it runs on any Python here. Use Python 3.10 at
C:\\Users\\Intel\\AppData\\Local\\Programs\\Python\\Python310\\python.exe if you
also want numpy available for the other audio tools.
"""
from __future__ import annotations

import argparse
import collections
import csv
import os
import re
import struct
import sys

# Ordered: the first category whose words match wins, so "metal door" lands in
# DOOR rather than METAL. Keep the specific categories above the generic ones.
CATEGORIES: list[tuple[str, tuple[str, ...]]] = [
    ("footsteps", ("footstep", "foot step", "footfall", "walk", "run ", "running", "jog",
                   "boot", "shoe", "sneaker", "barefoot", "step_", "steps")),
    ("body_cloth", ("cloth", "fabric", "rustle", "jacket", "clothing", "gear", "backpack",
                    "pocket", "denim", "leather creak")),
    ("body_breath", ("breath", "breathe", "breathing", "inhale", "exhale", "gasp", "pant",
                     "wheeze", "heartbeat", "heart beat", "pulse")),
    ("voice", ("whisper", "scream", "shout", "groan", "moan", "grunt", "vocal", "voice",
               "cry", "laugh", "mumble", "chant")),
    ("creature", ("creature", "monster", "beast", "zombie", "demon", "growl", "snarl",
                  "roar", "hiss")),
    ("door", ("door", "hinge", "latch", "handle", "knob", "gate", "hatch", "lock")),
    ("metal_stress", ("creak", "strain", "groan metal", "metal stress", "girder", "beam",
                      "chain", "pipe", "rebar", "scaffold", "rust", "screech")),
    ("water", ("drip", "splash", "puddle", "water", "sewer", "leak", "flow", "bubble")),
    ("sea_wind", ("sea", "ocean", "wave", "surf", "shore", "beach", "wind", "gust", "howl",
                  "storm", "rain")),
    ("impact_debris", ("impact", "slam", "thud", "crash", "debris", "rubble", "collapse",
                       "smash", "break", "fall", "drop", "hit", "bang", "knock")),
    ("surface", ("concrete", "stone", "gravel", "grit", "dirt", "sand", "wood", "tile",
                 "glass", "metal grate", "grate")),
    ("electrical", ("electric", "electricity", "spark", "buzz", "fluorescent", "neon",
                    "switch", "click", "power", "generator", "transformer", "hum")),
    ("tension", ("drone", "dark", "horror", "tension", "eerie", "scary", "creepy", "riser",
                 "stinger", "braam", "whoosh", "sub", "rumble", "atmosphere", "atmos",
                 "cinematic", "suspense")),
    ("industrial", ("machine", "industrial", "factory", "engine", "motor", "pump", "fan",
                    "ventilation", "conveyor")),
    ("roomtone", ("room tone", "roomtone", "ambience", "ambiance", "ambient", "interior",
                  "hall", "corridor", "tunnel", "basement", "warehouse")),
]


def categorise(text: str) -> str:
    low = text.lower()
    for name, words in CATEGORIES:
        if any(w in low for w in words):
            return name
    return "other"


def read_wav_info(path: str) -> dict:
    """Parse RIFF chunks directly: the wave module chokes on library extras."""
    info = {"sample_rate": 0, "channels": 0, "bits": 0, "seconds": 0.0, "description": ""}
    with open(path, "rb") as f:
        riff = f.read(12)
        if len(riff) < 12 or riff[:4] != b"RIFF" or riff[8:12] != b"WAVE":
            return info
        byte_rate = 0
        while True:
            header = f.read(8)
            if len(header) < 8:
                break
            chunk_id, size = struct.unpack("<4sI", header)
            data = b""
            if chunk_id in (b"fmt ", b"bext", b"iXML", b"LIST"):
                data = f.read(size)
            else:
                f.seek(size, os.SEEK_CUR)

            if chunk_id == b"fmt " and len(data) >= 16:
                _fmt, channels, rate, byte_rate, _align, bits = struct.unpack("<HHIIHH", data[:16])
                info.update(channels=channels, sample_rate=rate, bits=bits)
            elif chunk_id == b"data":
                if byte_rate:
                    info["seconds"] = round(size / byte_rate, 3)
            elif chunk_id == b"bext" and len(data) >= 256:
                info["description"] = data[:256].split(b"\x00")[0].decode("latin-1", "replace").strip()
            elif chunk_id in (b"iXML", b"LIST") and not info["description"]:
                text = data.decode("latin-1", "replace")
                match = re.search(r"<BWFXML>.*?<DESCRIPTION>(.*?)</DESCRIPTION>", text, re.S | re.I)
                if match:
                    info["description"] = match.group(1).strip()

            if size % 2:  # RIFF chunks are word aligned
                f.seek(1, os.SEEK_CUR)
    return info


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", help="folder to scan")
    parser.add_argument("-o", "--out", default="library_inventory.csv")
    args = parser.parse_args()

    rows = []
    per_category = collections.Counter()
    seconds_per_category = collections.Counter()

    for dirpath, _dirs, files in os.walk(args.root):
        for name in files:
            if not name.lower().endswith(".wav"):
                continue
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, args.root)
            try:
                info = read_wav_info(path)
            except OSError as exc:
                print(f"!! {rel}: {exc}", file=sys.stderr)
                continue

            category = categorise(rel + " " + info["description"])
            per_category[category] += 1
            seconds_per_category[category] += info["seconds"]
            rows.append({
                "category": category,
                "path": rel,
                "seconds": info["seconds"],
                "sample_rate": info["sample_rate"],
                "channels": info["channels"],
                "bits": info["bits"],
                "mb": round(os.path.getsize(path) / 2 ** 20, 2),
                "description": info["description"][:300],
            })

    rows.sort(key=lambda r: (r["category"], r["path"]))
    with open(args.out, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else
                                ["category", "path", "seconds", "sample_rate", "channels", "bits", "mb", "description"])
        writer.writeheader()
        writer.writerows(rows)

    print(f"{len(rows)} wav files -> {args.out}\n")
    print(f"{'category':16s} {'files':>6s} {'minutes':>9s}")
    for category, count in per_category.most_common():
        print(f"{category:16s} {count:6d} {seconds_per_category[category]/60:9.1f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
