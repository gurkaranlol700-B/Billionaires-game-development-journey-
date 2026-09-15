# SEAWALL — Audio handoff (written 2026-09-15)

This file is for a **new Claude Code session** picking up the sound design. Read it fully
before touching anything. The full project history is in `JOURNAL-FULL.md` (same folder) and
live at https://billionaires-game-development-journ.vercel.app/ — but **today's audio work is
only recorded here**, not in the journal.

---

## 1. Who you are working with

- The user is a **beginner** learning Unreal Engine 5. Explain the plumbing (what a thing is and
  why it exists), not just what to type. They like having **dials** they can tune themselves.
- They prefer momentum over long investigations: verify quietly, report results plainly, and
  don't narrate every intermediate check.
- Target feel: **Resident Evil Village / Outlast** horror sound — dizzying, oppressive, real.
- Passwords and API keys never pass through Claude. The user types their own logins.

## 2. Project basics

| Thing | Where |
|---|---|
| Unreal project | `D:\UnrealProjects\Seawall\Seawall.uproject` (UE 5.8, C++ + Blueprint) |
| Engine | `C:\Program Files\Epic Games\UE_5.8` |
| Level | `/Game/Maps/L_Corridor_01` (corridor + 4 side rooms) |
| Player | C++ `ASWCharacter` in `Source/Seawall/{Public,Private}/Player/` |
| Git | repo root = project folder, GitHub `gurkaranlol700-B/Billionaires-game-development-journey-`, branch `main`, Git LFS for binaries (`*.uasset`, `*.umap`, `*.wav` already tracked) |
| Journal site | the `*.html` files in the repo root, auto-deploy to Vercel on push |

### Starting a session (this has bitten three sessions in a row)

**Open Unreal first, wait until it finishes loading, THEN start Claude Code.** The Unreal MCP
server (`http://127.0.0.1:8000/mcp`) is only checked once, at session start. If Unreal was closed
then, the `mcp__unreal__*` tools never appear — launching Unreal later does not fix it. Restart
Claude Code with Unreal open.

## 3. Audio — what is DONE (verified)

### Generated sounds (made with Python/numpy/scipy, not recorded)

Sources in `Art/Audio/Source/`, imported copies in `Content/Audio/Ambience/`:

| Unreal asset | Length | What it is | Verified |
|---|---|---|---|
| `/Game/Audio/Ambience/SW_Amb_RoomTone_Corridor` | 30 s | Pink noise, low-passed at 380 Hz, slow breathing swell. 83% of energy under 300 Hz | Loop seam 0.4% of peak |
| `/Game/Audio/Ambience/SW_Hum_Fluorescent_Steady` | 10 s | 100 Hz mains hum + 12 harmonics + ballast buzz (2.5–7 kHz, pulsing 100×/s). Exactly 1000 cycles so it loops perfectly | Peaks at 100/200/300 Hz |
| `/Game/Audio/Ambience/SW_Hum_Fluorescent_Faulty` | 10 s | Same hum + brown-outs + arcing crackle — for the broken, flickering fixture | Same |

- All three are **mono, 48 kHz**, imported as SoundWave, **`bLooping` = true** (set and read back).
- **Nobody has listened to them yet.** Claude can't hear. First thing: have the user play each in
  the Content Browser and judge them. Regenerate with different parameters if they sound wrong.

### Import route that works

Unreal's auto-import watches `Content/`. Copying a `.wav` into e.g. `Content/Audio/Footsteps/Concrete/`
creates the SoundWave asset automatically and saves it (confirmed in the log via `SoundFactory`).
There is **no audio import tool in the Unreal MCP** — this file-drop route is the way in.

**Editor setting changed:** `Editor Preferences > General > Loading & Saving > Prompt Before Auto
Importing` was set to **false** so imports don't need a click. Restore it to `true` when the audio
pass is finished (ConfigSettingsToolset, container `Editor`, category `General`, section
`LoadingSaving`, property `bPromptBeforeAutoImporting`).

## 4. Audio — HALF DONE (be careful)

### Footstep playback code — written, NEVER COMPILED, not committed

Changed files (uncommitted in the working tree):
- `Source/Seawall/Public/Player/SWCharacter.h` — new `Seawall|Audio` properties:
  `FootstepSounds` (array), `FootstepSoundFolder` = `/Game/Audio/Footsteps/Concrete`,
  `FootstepVolumeWalk` 0.55 / `Sprint` 1.0 / `Crouch` 0.18 / `Land` 1.1,
  `FootstepPitchVariance` 0.06, `FootstepVolumeVariance` 0.12; methods `PlayFootstepSound`,
  `LoadFootstepSoundsFromFolder`; state `LastFootstepIndex`.
- `Source/Seawall/Private/Player/SWCharacter.cpp` — loads every sound in the folder at BeginPlay if
  the array is empty (asset registry scan), plays a random take with no immediate repeats and slight
  pitch/volume variation on each step (from `UpdateFootsteps`) and on landing.
- `Source/Seawall/Seawall.Build.cs` — added `"AssetRegistry"` to private dependencies.

**First job:** close Unreal, build (`mcp__seawall__build`, target `SeawallEditor`), fix any errors,
reopen Unreal, then restart Claude Code (see section 2). New UCLASS/UPROPERTY changes cannot be
hot-reloaded with Live Coding.

Known limitation to note later: sounds found by folder scan are not referenced, so a **packaged**
build will need that folder added to *Additional Asset Directories to Cook* (milestone M9).

### Footstep recordings — chosen, not downloaded

freesound.org, both **CC0** (public domain, commercial use OK, no credit required), both WAV:

| ID | Title | Why | Download path (needs login) |
|---|---|---|---|
| 813622 | Footsteps – Stone, Rock, Concrete, Cement (SecureSubset) | Foley pit, Sennheiser MKH416, clean | `/people/SecureSubset/sounds/813622/download/813622__securesubset__footsteps-stone-rock-concrete-cement.wav` |
| 546827 | Footsteps Leather Concrete (Kinoton) | Leather soles on gritty concrete, slow AND fast walk, MixPre-6 | `/people/Kinoton/sounds/546827/download/546827__kinoton__footsteps-leather-concrete.wav` |

The user **just created a freesound account on 2026-09-15** — it needs the confirmation email link
clicked before they can log in and download. Chrome is driven through the `chrome-devtools` MCP
(the user's real, logged-in browser).

Plan once downloaded: slice each recording into individual steps (onset detection, trim silence,
short fade-out, mono, normalise), save as `SW_Step_Concrete_01.wav`, `_02`… into
`Content/Audio/Footsteps/Concrete/`. Aim for 8–12 takes. Auto-import does the rest.

## 5. Audio — NOT STARTED

### Placing the loops in the level

Use `AmbientSound` actors (`/Script/Engine.AmbientSound`), each with an `AudioComponent`.
Property names on the component (read from a live actor, so they are exact):
- `Sound` (object ref), `VolumeMultiplier`, `PitchMultiplier`, `bAutoActivate`,
  `bAllowSpatialization` (set false for a flat 2D room tone)
- `bOverrideAttenuation` (bool) and `AttenuationOverrides` (struct) with fields
  `bAttenuate`, `bSpatialize`, `attenuationShape` ("Sphere"), `attenuationShapeExtents` {x = inner
  radius}, `falloffDistance`, `distanceAlgorithm` ("Linear" default), `bEnableOcclusion`,
  `bEnableReverbSend`. Defaults are inner 400 / falloff 3600 — far too big for a hum.

Suggested layout (all positions in cm):
- Room tone: one actor, 2D, low volume, anywhere in the corridor (e.g. 900, 0, 200).
- Steady hum just below each glowing ceiling panel (panels hang from the 400 cm ceiling):
  `LF_Corridor_0` (250, 0), `LF_Corridor_2` (1620, 850), `LF_Room_R1` (475, −340),
  `LF_Room_R2` (1075, −340), `LF_Room_R3` (475, 340), `LF_Room_R4` (1075, 340) — z ≈ 370.
- **Faulty hum** on `LF_Corridor_1` (1350, 0, ≈370) — the broken, flickering fixture.
- Give each hum a slightly different `PitchMultiplier` (0.97–1.03) so identical loops don't comb.
- Something like inner radius 80, falloff 600, volume 0.25–0.4 is a starting point — let the user
  tune it by walking.

Level geometry for reference: corridor A x 0–1800, y ±180, floor 0, ceiling 400; segment B
x 1440–1800, y 180–1200; doorways into four side rooms whose back walls are at y ±490.

### After audio: re-measure performance

Current baseline with Lumen hardware ray tracing: **123 fps average, 89 fps 1% low** at 1080p
(8.14 ms/frame). Method and numbers: `Profiling/Baselines/*.md`. Re-run the same capture once
audio is in and compare.

## 6. MCP traps that cost real time

- `ObjectTools.set_properties` needs `values` as a **JSON string**, and returns `true` even when
  nothing changed. **Always read the value back.**
- `get_properties` returns a JSON **string** — `json.loads` it.
- Light `LightColor` reads/writes as **0–1 floats**, not 0–255 (255 silently clamps to white).
- `execute_tool_script` sandbox: dicts don't support `.get(key, default)`; use `key in d`.
- `CaptureViewport` returns base64 too big for the tool channel; it's saved to a tool-results file.
  `Tools/shot.py` decodes the newest one to PNG.
- `UnrealEditor.exe ... -game` writes logs and profiling CSVs to
  `%LOCALAPPDATA%\UnrealEngine\5.8\Saved\`, not the project's `Saved\`.
- Lumen silently switches itself off without `r.RayTracing` + SM6 (or distance fields). Fixed on
  2026-09-14; verify with the log line `Ray tracing is enabled`.
- C: drive runs close to full. Check `df -h /c` at session start.

## 7. State of the repo at handoff

- Last commits: lighting approved (`0762a10`) and this handoff.
- Committed today: the three generated sounds (sources + imported assets), this file,
  `JOURNAL-FULL.md`.
- **Uncommitted on purpose:** the footstep C++ changes (never compiled). Build first, then commit.
- The corridor map may show as modified in the editor: an empty AmbientSound was spawned and
  removed again. If Unreal asks to save `L_Corridor_01`, saving or not saving are both fine.
