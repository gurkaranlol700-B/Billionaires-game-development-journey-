# SEAWALL - complete build journal (plain-text export)

Exported 2026-09-15 from the journal pages in this folder.
Live site: https://billionaires-game-development-journ.vercel.app/
Work done after Session 05 (the first audio pass) is in HANDOFF-AUDIO.md, not here.


==============================================================================
# PAGE: Session 01 - The Build Log  (index.html)
==============================================================================

Build Log · Session 01 · 5 Sep 2026

# SEAWALL

A first-person survival horror game set on Kurogane-jima, a fictional abandoned concrete mining island off Nagasaki. Four to five hours, scarce combat, built in Unreal Engine 5.8. This is the working record — every decision, every thing that broke, and how it got fixed.

EngineUE 5.8.2

ToolchainVerified

Projects5 / 5 safe

NextM1


## What we're making

Your father, a marine surveyor, vanished on Kurogane-jima four months ago. The search was called off. You and Kaito — his student, your closest friend, the last person to see him — hire a boat and land on the island at dusk.

Act I Escape

The boat is gone within the hour. You find Kaito pinned in a collapsed stairwell and get him out — it costs you. Every route off the island fails: the pier is gone, the radio needs power, the power needs the pump house, the pump house is flooded. This act teaches the horror loop by making escape the only thing you want and denying it four times.

Act II Save someone

Your father's recorder turns up in the flooded pump house. He's alive, and he's down — in the shafts under the seabed. Your goal inverts: stop trying to get out, start going deeper. Kaito comes with you. He tells you about his sister.

Act III The truth

Your father isn't a prisoner. He's the one who's been feeding it. The island gives things back, and it charges for them. Kaito's sister has been dead for two years. She has been answering his phone for one. He was always going to trade you — and he saved you in Act II so you'd be worth something in Act III.


### Decisions locked

| Decision | Choice | Why |
|---|---|---|
| Perspective | First person | 3–5× less animation work; the monster is only where you look |
| Setting | Kurogane-jima | No horror game has used Gunkanjima; free Megascans concrete fits exactly |
| Combat | Scarce, RE Village style | Only option that supports real boss fights |
| Betrayal | Understandable (TLOU register) | You'd have done the same — that's what makes it sit in your chest |
| Code | C++ core, Blueprint content | Text files I can author; replication-safe for co-op later |
| Length | 4–5 hours | Scope cap. Everything else is a parked list, not a build item |

Deliberate change

The real Hashima Island was a site of Korean and Chinese forced labour during WWII, and people died there. Building a monster shooter on a real atrocity is both tasteless and a genuine product risk. So the island is fictional: Kurogane-jima (黒鉄島, "Black Iron Island") is visually Gunkanjima — same brutalist concrete city in the sea, same seawall, same shafts under the seabed — but it's ours, and we can invent whatever history the story needs.


## Session 01 — laying the ground

01


### The hardware audit

Done

Before promising anything, we established what the machine can actually do.

| Part | Spec | Verdict |
|---|---|---|
| GPU | RTX 5060 Ti 16GB | Lumen + Nanite + hardware RT + DLSS 4. Comfortable at 1080p60. |
| CPU | i5-14400F · 10C / 16T | Shader compiles are CPU-bound. Fine, not fast. |
| RAM | 32 GB | The weak link. UE5 prefers 48–64. Close ComfyUI before opening Unreal. |
| Disk | 2TB NVMe (C/D/F/G) | All one physical drive — so D: costs zero speed versus C:. |

The disk situation was the first real problem: C: had 18 GB free. Unreal generates enormous derived data — shader caches, cooked content, intermediates — and would have filled that within a week. Everything Unreal now lives on D:, which had 358 GB.

02


### Migrating five projects off C:

Done

Five existing projects sat in Documents\Unreal Projects. The key discovery: only 1.14 GB of their 7.81 GB was real. This is true of every Unreal project you will ever make.

| Folder | Keep | What it is |
|---|---|---|
| Content/ | YES | Your actual assets — meshes, materials, Blueprints, levels |
| Config/ | YES | Project settings (.ini) |
| Source/ | YES | C++ code, if the project has any |
| Binaries/ | no | Compiled output — rebuilt on demand |
| Intermediate/ | no | Build scratch space |
| DerivedDataCache/ | no | Cached shaders and textures |
| Saved/ | check | Logs and autosaves — but SaveGames and Screenshots are yours |

We checked SaveGames and Screenshots on all five: empty. Then copied and verified by file count and byte size:

```
PROJECT                  SRC_FILES DST_FILES    SRC_MB    DST_MB  STATUS
car_game_5hrs                  526       526       607       607  SAFE
landscape_1                    274       274       242       242  SAFE
masterclass_game               265       265       137       137  SAFE
my_first_unreal_game           776       776       146       146  SAFE
real_npc_car_ai                217       217        50        50  SAFE

>>> ALL 5 PROJECTS VERIFIED SAFE ON D:
```

Why copy, never move

Upgrading a Blueprint project from 5.7 to 5.8 is a one-way conversion. If it breaks, you want the original still sitting there. All five are Blueprint-only (no Source/), which makes this about as low-risk as Unreal upgrades get — no code to recompile, just asset format conversion.

03


### The toolchain fight

Fixed

Unreal C++ does not compile just because "Visual Studio is installed." We tested this on day one with a six-file throwaway project, on purpose, rather than discovering it in week three with real work in flight. It failed:

```
Available x64 toolchains (1):
 * ...\VC\Tools\MSVC\14.50.35717

Visual Studio 2026 compiler version 14.50.35725 is not a preferred version.
Unable to instantiate module 'SwarmInterface': Could not find NetFxSDK install dir

Result: Failed (RulesError)
```

Two separate things. The compiler-version line is a warning — harmless. The blocker was that the machine had a C++ compiler but no .NET Framework SDK at all. Until fixed, no Unreal C++ project could build here.

Unreal writes its exact requirements in a readable file — worth knowing this exists:

```
<Engine>\Engine\Config\Windows\Windows_SDK.json

  "MinimumVisualCppVersion":  "14.38.33130"
  "BannedVisualCppVersions":  [ "14.44.0-14.44.35210", "14.40.0-14.43.99999", ... ]
  "PreferredVisualCppVersions": [ "14.44.35207-14.44.99999" ]
```

It even lists banned compiler versions with known codegen bugs. Ours was above minimum and not banned — just not preferred.


### The subtle bit that cost a round trip

".NET Framework targeting pack" and ".NET Framework SDK" are different components. Installing the targeting pack put files in Reference Assemblies\ — but Unreal doesn't look there. Reading UnrealBuildTool's own C# source settled it:

```
// MicrosoftPlatformSDK.cs
string[] PreferredVersions = new string[] { "4.6.2", "4.6.1", "4.6" };
string NetFxSDKKeyName = "Microsoft\\Microsoft SDKs\\NETFXSDK";
TryReadInstallDirRegistryKey32(NetFxSDKKeyName + "\\" + PreferredVersion,
                               "KitsInstallationFolder", out OutInstallDir)
```

It reads a registry key, which only the SDK component creates. Fix: Microsoft.Net.Component.4.6.2.SDK — .SDK, not .TargetingPack. We added MSVC 14.44 at the same time to get onto the toolchain Epic actually tests. Result:

```
Using Visual Studio 2026 14.44.35223 toolchain and Windows 10.0.26100.0 SDK.

[3/7] Compile [x64] ToolchainTest.cpp
[6/7] Link    [x64] UnrealEditor-ToolchainTest.dll

Result: Succeeded          Total execution time: 66.90 seconds
```

Note it picked 14.44 by itself and the "not preferred" warning is gone.

Lesson

When a build tool says it can't find something, find out where the tool looks. Unreal ships UnrealBuildTool's full C# source under Engine\Source\Programs\UnrealBuildTool — you can always just read it instead of guessing.

Keep this

D:\UnrealProjects\ToolchainTest is six files that compile in ~60 seconds, which makes it the fastest possible answer to "is the engine/compiler setup healthy?" It gets re-run against every new engine version.

04


### Two MCP servers

Written

Epic's built-in Unreal MCP, shipped in UE 5.8 (June 2026) — this is the entire reason for the engine upgrade. It embeds an MCP server inside the editor process, so an AI agent can spawn actors, configure lighting, build material instances and run automation tests.

```
Plugins:  Unreal MCP, All Toolsets   (Toolset Registry follows automatically)
Prefs:    General > Model Context Protocol > Auto Start Server
Console:  ModelContextProtocol.GenerateClientConfig ClaudeCode

Binds     http://127.0.0.1:8000/mcp   loopback only, no auth, experimental
```

Our own seawall-mcp covers what Epic's doesn't. Epic's server drives the editor; it can't compile, read logs, or show me the game. Same architecture as the Blender MCP — a PEP 723 header declares its own dependencies so uv run server.py needs no venv — but with no socket at all, which makes it much harder to break.

| Tool | What it's for |
|---|---|
| project_info() | Sanity check — engine path, modules, ever been built |
| build() | Compile, returning only the error lines (raw logs are ~50k lines of noise) |
| tail_log() | Read Saved/Logs/Seawall.log — where every UE_LOG lands |
| find_in_log() | Regex search the log with context |
| run_tests() | Headless automation tests |
| latest_screenshot() | Let me actually see the game |

05


### Git, with LFS from commit zero

Done

.gitattributes exists before the first binary asset. This ordering is non-negotiable: .uasset and .umap are binary, and Git stores a complete new copy on every change — a 200 MB level edited fifty times becomes 10 GB of unusable history. LFS keeps them outside the repo and stores a pointer instead. Adding it afterwards means rewriting history.

06


### The install that couldn't be fixed by freeing space

Fixed

The 5.8 install kept failing with a dialog saying "try to free up space on your drive and attempt to install again", error code IS-IN-DS01. The launcher log confirmed DS = disk space:

```
LogEoshPatcher: BpsEndStats.ErrorCode: 'DS01'
LogDownloadManager: AppId [ue:...:UE_5.8] AlertCode=[IS-IN-DS01] IncompleteInstall=1
LogAutoUpdateService: The installation failed ... the installer will no longer try
                      to perform an automatic update again until the user specifies it
```

But freeing space would not have fixed it, and that's the useful part. The launcher had written a pending record that hard-pinned the destination:

```
...\Manifests\Pending\<hash>.item

  InstallLocation       C:\Program Files\Epic Games\UE_5.8   <-- pinned
  InstallSize           31693779023   (31.7 GB)
  bIsIncompleteInstall  True
```

Because a half-finished install existed, the Library offered Resume instead of a fresh install dialog — so there was no drive picker to change. Every retry went back to C: and failed identically. The log even refused outright: AppState=[AppPatching] not installable ... + cancelling.

The fix was to delete the stuck record. We verified the folder held exactly one file — a 35 MB manifest, zero engine binaries — before removing anything.

| Path | What it holds |
|---|---|
| ...\EpicGamesLauncher\Data\Manifests\*.item | One JSON per app — install path, version, size |
| ...\Manifests\Pending\ | Queued or half-finished installs — the thing that gets stuck |
| ...\UnrealEngineLauncher\LauncherInstalled.dat | What the launcher believes is installed |
| %LOCALAPPDATA%\EpicGamesLauncher\Saved\Logs\ | The actual error codes |

With the record cleared and 5.7 uninstalled (reclaiming 26.6 GB, taking C: from 16 GB to 42 GB free), the install finally had both a clean slate and room to land.

07


### Keeping the shader cache off C:

Done

Unreal's derived data cache — compiled shaders, converted textures — routinely reaches 10–50 GB. By default it lands in %LOCALAPPDATA%, i.e. C:, which after the engine install has roughly 10 GB spare. Redirected before the engine ever ran, so there was nothing to migrate:

```
[Environment]::SetEnvironmentVariable(
    'UE-LocalDataCachePath', 'D:\UnrealDDC\Local', 'User')
```

Combined with every project living on D:, the only thing left on C: is the engine itself. This is what makes a C: engine install survivable rather than a slow-motion problem.

08


### 5.8 lands, and a small vindication

Done

UE 5.8.2 finished installing. C: settled at 11 GB free — almost exactly the ~10 GB predicted. Epic's MCP stack is all present:

```
Experimental/ModelContextProtocol/ModelContextProtocol.uplugin
Experimental/ToolsetRegistry/ToolsetRegistry.uplugin
Experimental/Toolsets/AllToolsets/AllToolsets.uplugin
```

There are around 30 individual toolsets the editor MCP can drive — including GASToolsets, StateTreeToolset, NiagaraToolsets, UMGToolSet, AutomationTestToolset, LiveCodingToolset and MetaHumanGenerator. Nearly everything Seawall needs is directly drivable.

Re-ran the six-file smoke test against the new engine:

```
Using Visual Studio 14.50.35725 toolchain and Windows 10.0.26100.0 SDK.

[4/7] Compile [x64] ToolchainTest.cpp
[6/7] Link    [x64] UnrealEditor-ToolchainTest.dll

Result: Succeeded          Total execution time: 76.98 seconds
```

Note what it picked: 14.50, with no "not preferred" warning. 5.8 widened its accepted range:

```
"PreferredVisualCppVersions": [
    "14.50.35717-14.50.99999",      <-- now first choice
    "14.44.35207-14.44.99999"
]
```

Worth knowing

The original compiler was always going to be fine on 5.8 — that warning was purely a 5.7 artifact, and installing 14.44 turned out to be unnecessary (harmless, and now a fallback). The .NET Framework SDK fix was the one that actually mattered, and it would have blocked us on any engine version. Worth separating "a warning" from "a blocker" before spending effort on either.

All five migrated projects then bumped to "EngineAssociation": "5.8".

09


### This journal, on the internet

Live

The journal now publishes itself. It lives in the repo as index.html — a single file, no build step, no dependencies beyond Google Fonts — so Vercel serves it straight from the repository root with the Other preset and empty build settings.

```
git push  →  GitHub  →  Vercel auto-deploy  →  live in ~30s
```

Every future session appends here and pushes; the site updates itself. Readable from a phone, anywhere, without needing the machine that's building the game.

10


### The project itself

Compiles

Seawall is a clean C++ module, not a template copy. Unreal's First Person template ships a gun and a projectile — the opposite of what this game wants — so starting from it would only mean deleting things. The character gets written properly in C++ at M1 instead.

Wired in from the first commit, because retrofitting any of it is expensive:

| Module | Why now rather than later |
|---|---|
| GameplayAbilities | GAS is replication-native — this is what keeps co-op cheap at M10 |
| EnhancedInput | Action/context based input rather than raw key binds |
| StateTree, AIModule | Creature behaviour; StateTree is what Epic is moving to over Behavior Trees |
| NavigationSystem | Pathing for everything that hunts you |
| LogSeawall | Our own log category, so our messages filter out of Unreal's very noisy log |


### Rendering defaults, chosen for the look

```
r.DynamicGlobalIlluminationMethod=1   Lumen GI
r.Lumen.HardwareRayTracing=1         the 5060 Ti has the hardware, so use it
r.Nanite.ProjectEnabled=True         Megascans at full density, no triangle budget
r.Shadow.Virtual.Enable=1            torch beams across rubble read correctly
r.AntiAliasingMethod=4               TSR upscaling for headroom
r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True
```

That last line matters more than it looks: without extended luminance range an unlit corridor comes out washed-out grey instead of genuinely dark. In a game whose whole tension depends on not being able to see, that is the difference between scary and cheap.

```
[6/7] Link [x64] UnrealEditor-Seawall.dll
Result: Succeeded          Total execution time: 53.00 seconds
```

11


### The MCP SDK moved under us

Fixed

Testing the server rather than assuming it worked caught this immediately:

```
ModuleNotFoundError: No module named 'mcp.server.fastmcp'.
This is mcp 2.x, where FastMCP was renamed to MCPServer
(from mcp.server.mcpserver import MCPServer) and other APIs changed
```

Two ways out: pin mcp<2 and stay on a dead API, or migrate. We did neither exactly — the import now tries the new name and falls back to the old, so the same file runs on either SDK version:

```
try:
    # mcp 2.x
    from mcp.server.mcpserver import MCPServer as _Server, Image
except ImportError:
    # mcp 1.x
    from mcp.server.fastmcp import FastMCP as _Server, Image
```

This affects blender-mcp too

The Blender MCP server has the identical pattern — dependencies = ["mcp>=1.2.0"] with no upper bound, importing mcp.server.fastmcp. It works today only because uv cached an old resolution. On a fresh machine, or through RESTORE.ps1, it would fail the same way. The same three-line fix applies.

Then project_info() earned its keep on first run by catching a bug in its own author's work — the engine path had been defaulted to D: while the engine actually landed on C::

```
Engine root  : D:\Epic Games\UE_5.8  (MISSING)
Build.bat    : MISSING
```

It now probes candidate paths in order, so moving the engine to D: later needs no edit:

```
Engine root  : C:\Program Files\Epic Games\UE_5.8  (found)
Build.bat    : found
C++ modules  : Seawall
Editor binary: built
```

Lesson

A "sanity check" tool is worth writing first, not last. Six tools registered and the very first call surfaced a wrong path that would otherwise have shown up as a confusing build failure days later.


## Where things stand

- Hardware audited; disk strategy set (everything Unreal on D:)

- 5 projects copied to D:\UnrealProjects\ — verified by file count and byte size

- C++ toolchain fixed (.NET Framework 4.6.2 SDK + MSVC 14.44) — verified by a real compile and link

- seawall-mcp server written and syntax-checked

- git + LFS configured before any binary asset

- Stuck Epic install record cleared

- UE 5.7 uninstalled — reclaimed 26.6 GB

- DDC redirected to D:\UnrealDDC\Local

- UE 5.8.2 installed — C: settled at 11 GB free, as projected

- ToolchainTest re-verified against 5.8 — succeeded in 77s

- All 5 projects bumped to EngineAssociation 5.8

- Journal published to GitHub and auto-deploying on Vercel

- Verify all 5 migrated projects open in 5.8 — your job, you know what they should look like

- Delete the C: originals — another 7.9 GB

- Enable the Unreal MCP plugins; generate the Claude Code config

- Create the Seawall project; wire both MCP servers

- M1 — a corridor you're afraid to walk down

Known constraint

Git LFS on a free GitHub account allows 1 GB of storage and 1 GB of bandwidth per month. Nothing binary has been committed yet, so this costs nothing today — but Unreal assets will pass that quickly once M4 starts building the island. The decision to make before then: pay for LFS data packs, or keep Content/ out of GitHub and back it up to the existing G:\ mirror instead.


## The road from here

| # | Milestone | Days | Ends with |
|---|---|---|---|
| M1 | Walk the corridor | 2–3 | A hallway that is genuinely frightening |
| M2 | The Hunter | 4–5 | Noise propagation, the Watchman, hiding — the core loop |
| M3 | Survival systems | 5–7 | Inventory, locks, puzzles, ammo economy |
| M4 | The Island | 10–14 | Pier, blocks, school, pump house, shafts |
| M5 | The Bestiary | 10–14 | Shiftmen, Drowned, Hollow, The Voice + AI Director |
| M6 | Bosses | 5–7 | Three encounters, each its own arena mechanic |
| M7 | Story | 10–14 | Kaito, cutscenes, the three acts, the betrayal |
| M8 | Art & audio | 14–21 | The "a solo dev made this?" pass |
| M9 | Product | 7–10 | Menus, saves, packaging, 60fps, Steam page |
| M10 | Co-op | later | Two players over EOS |

Honest total to a shippable build: roughly three to four months of steady work. Every milestone above is playable on its own, so there's never more than a week between something you can show someone.

The rule that protects all of it

Scope creep is what kills solo games. 4–5 hours is the cap. Every "what if we also…" goes on a parked list, not into the build. Separately, everything is written server-authoritative from the first line of C++ — single player is just a one-player listen server — because retrofitting co-op into a finished game is a rewrite, and doing it right now costs almost nothing.

Next page The Workstation Which tool does which job, the three asset routes, where sound comes from, and the performance budget we hold from day one. →

SEAWALL · build log Session 01 · 5 Sep 2026

==============================================================================
# PAGE: Workstation  (pipeline.html)
==============================================================================

Seawall · Working Method

# THE WORKSTATION

Which tool does which job, how assets travel from an idea to the island, where sound comes from, and the performance discipline that starts on day one rather than the month before shipping.

Frame budget16.6 ms

Target1080p / 60

Build firstWorld

Never togetherUE + ComfyUI


## What to build first

You asked: combat, environment, or world? The world — and specifically one corridor, finished to shipping quality. Four reasons, in order of how much they matter:

- Horror lives in the room, not the fight. Resident Evil Village and Outlast are frightening because of where you are and what you hear. Combat is the release of tension, never the source of it. Build the source first.

- Combat cannot be tuned in a vacuum. Weapon feel, enemy spacing, how much ammo is "scarce" — all of it is a function of the space you fight in. Tuning combat in a grey box means re-tuning it later from scratch.

- It de-risks the biggest unknown earliest. The ComfyUI → Blender → Unreal pipeline is the part most likely to disappoint. Finding that out in week one on a single corridor is cheap; finding out in month three across a whole island is not.

- The noise system needs geometry. Sound occlusion — the mechanic the whole game rests on — is meaningless without real walls to muffle through.

The trap to avoid

"Build the world first" does not mean blocking out the whole island. It means one corridor taken all the way: final materials, final lighting, final sound, running at 60fps. That corridor becomes the quality bar and the performance budget for everything after it. A whole island of grey boxes teaches you nothing; one finished corridor teaches you everything.


## Which tool for which job

The rule underneath this table: stay in Unreal unless you have a reason to leave. Every round trip through another program costs time and introduces a chance to break something.

| Job | Tool | Why that one |
|---|---|---|
| Concrete, rust, pipes, rubble, generic props | Fab / Megascans | Free, photo-scanned, Nanite-ready. Always check here first — making by hand what you can download is the most common way solo projects die |
| Blockout, level geometry, simple shapes, booleans | UE5 Modeling Mode | Already in the editor. No export, no round trip. Most level geometry never needs Blender |
| Scattering debris, rubble fields, decay | UE5 PCG | Procedural, art-directable, and re-runnable when the layout changes |
| Concept art, mood boards, "what does this room feel like" | ComfyUI · Chroma | IMAGE-LAB-CHROMA-V2.json. Decide the look in minutes, not days of modelling |
| Signage, posters, decals, Shōwa-era paper, stains | ComfyUI → UE decals | Flat art is where image generation is genuinely strongest. Enormous set-dressing value per hour |
| Unique props Fab does not have (shrine, miner gear, machinery) | ComfyUI → Hunyuan3D → Blender | prompt-to-3d-FLUX.json. Background removal first, then retopologise in Blender — raw output is never game-ready |
| Creatures | ComfyUI concept → Hunyuan3D base → Blender | Sculpt, retopo, UV, rig, animate. You have already rigged a 16-bone character, so this path is proven |
| Humans — Kaito, your father, the Watchman | MetaHuman | Face and body quality you cannot hand-model in reasonable time. Installed already |
| Rigging, skinning, animation, retopology | Blender | The one job Unreal genuinely cannot do better |
| Gameplay, AI, systems, lighting, sound design | UE5 | All of it. This is where the game actually is |


## The asset pipeline

Every asset takes one of three routes. Prefer the top one; it is by far the cheapest.

```
ROUTE 1 — download            most environment art
   Fab / Megascans → UE5
   minutes. Nanite-ready, already photoreal.

ROUTE 2 — flat art             signage, decals, posters, grime
   ComfyUI (Chroma) → PNG → UE5 decal / material
   ~10 min per asset. Best value-per-hour in the whole pipeline.

ROUTE 3 — bespoke 3D          creatures, hero props, anything unique
   ComfyUI concept  →  background removal  →  Hunyuan3D
        →  Blender: retopo, UV, rig, animate
        →  FBX  →  UE5
   hours to days. Only when routes 1 and 2 genuinely cannot do it.
```

Non-negotiable rule

Hunyuan3D output is never game-ready. It produces dense, badly-topologised meshes with no UVs and no rig. It is a starting sculpt, not an asset. Skipping the Blender retopology step is how you end up with a 400k-triangle door handle that cannot be animated. Budget the Blender time, or use route 1 instead.


## Sound

You named the sound design of Resident Evil Village and Outlast as what makes you dizzy. That feeling is not an accident, and it is not expensive — it is the highest-impact, lowest-cost work in the entire project. Sound is roughly half of horror.


### Where it comes from

| Source | Use for | Cost |
|---|---|---|
| freesound.org (filter to CC0) | Sea, wind, metal groans, drips, debris, cloth | Free |
| Your own phone | Footsteps, doors, rummaging, breath, impacts on concrete | Free — and better than most libraries, because it is specific |
| Fab audio packs | Creature vocals, weapon foley, UI | Free rotations, else paid |
| A voice actor (Kaito, the tapes) | The performance the story rests on | The one place worth paying money |


### What makes it dizzying — MetaSounds

MetaSounds is Unreal's procedural audio graph, and it is the difference between "a wav file plays" and the effect you are describing. Four techniques carry almost all of it:

- Randomise everything. A drip that plays every 3.0 seconds at the same pitch becomes wallpaper in ten seconds. Randomise interval, pitch and volume slightly and the brain keeps treating it as real, which means it keeps paying attention.

- Layer ambience by depth. Sea against the seawall, wind through empty windows, structural groans, water in the shafts — four independent layers, each fading by how deep you are. Descending should sound like descending.

- Occlusion and reverb are the space. A sound through a concrete wall must be muffled and quieter, not just quieter. Combined with Audio Volumes for reverb, this is what makes a building feel solid — and it is the same data our noise system uses.

- Tie breath and heartbeat to fear state. Driven by proximity to a hunter, not by a scripted trigger. This is the single most effective horror audio trick there is: the player hears their own body betraying them before they see anything.

Sound is a mechanic here, not decoration

In Seawall the noise system is gameplay: what you hear tells you where the Watchman is, and what you emit tells him where you are. That means audio work is not a polish task to be done at M8 — the core loop needs it at M2. Rough sounds early beat perfect sounds late.


## Optimisation, from day one

You are right to raise this now. Unreal 5 makes it very easy to build something beautiful that runs at 22fps, and the reason it is hard to fix later is that by then the cost is spread across a thousand small decisions. The discipline is simple: a budget, measured continuously, on the worst-case scene.


### The budget

60fps means every frame must finish in 16.6 milliseconds. Not "the game runs at 60" — every single frame. Here is how that divides on your 5060 Ti at 1080p:

| Thread | Budget | What lives there |
|---|---|---|
| Game | < 6 ms | Our C++ and Blueprints, AI, physics, the noise system |
| Draw | < 6 ms | Preparing what to render. Driven by draw calls |
| GPU | < 13 ms | Lumen, Nanite, shadows, post. The usual bottleneck |
| Headroom | ~3 ms | Deliberately unspent. Frames are spiky; a budget with no slack is already over |


### How to measure — four commands

```
stat unit          the one that matters. Frame / Game / Draw / GPU in ms.
                   Whichever is closest to Frame is your bottleneck. Fix THAT.
stat gpu           GPU time broken down by pass -- shows if Lumen or shadows are the cost
stat scenerendering draw call count. Over ~3000 and the Draw thread is your problem
ProfileGPU         (Ctrl+Shift+,) one frame, every pass, sorted by cost
```

The rule that prevents most of the pain

Measure before optimising, always. The overwhelmingly common mistake is spending a week reducing triangle counts when the bottleneck was the Game thread the whole time. stat unit tells you which of the four numbers to care about. Optimising anything else is wasted work — and it is wasted work that also makes the game worse.


### The traps, specific to this game

| Trap | Why it bites a horror game | Discipline |
|---|---|---|
| Shadow-casting lights | Dark game → many small lights. Each shadow caster is a full extra render pass | Very few cast shadows. Your torch does; most emergency lights do not |
| Volumetric fog | Irresistible for atmosphere, and one of the most expensive things you can enable | Use it, but measure it. Keep the volume small and the resolution modest |
| Translucent overdraw | Dust motes, smoke, water sheets, glass — stacked translucency multiplies pixel cost | Watch it whenever particles fill the screen. This is the classic silent killer |
| Tick on everything | Hundreds of actors ticking every frame for something that changes once a second | Default to tick disabled. Use timers and events. This is a Game-thread killer |
| Nanite on the wrong things | Brilliant for solid geometry, bad for foliage-like or masked materials | Nanite for concrete and rubble; conventional LODs for anything with opacity masks |
| Lumen quality left on default | Enormous quality range and enormous cost range | Tune final-gather and reflection quality once, on the corridor, and lock it |


### The discipline

- A perf test map, from M1. One level that represents worst case — the most lights, the most geometry, the most particles the game will ever show at once. Every milestone gets measured there.

- Check stat unit at the end of every session. Thirty seconds. Catching a 2ms regression the day it appears is trivial; finding it three weeks later inside a hundred changes is not.

- Write the number in the journal. A performance graph over the project's life is the single most useful artefact for knowing whether you are drifting.

- The engine ships AutomatedPerfTestTools — already present in 5.8. Once the corridor exists we wire it up so performance is checked automatically rather than by remembering to.


## Running the machine

32 GB of RAM and 16 GB of VRAM is a good workstation, but it is not enough to run everything at once. This is the practical constraint that shapes your working day.

Hard rule

Never run ComfyUI and Unreal at the same time. Chroma alone holds ~8.5 GB of VRAM; Unreal with Lumen and Nanite wants most of the rest. Running both means both are slow and something eventually falls over mid-bake.

Which leads to a simple weekly shape — batch the tool switches, do not interleave them:

Mode A Asset days — ComfyUI + Blender

Unreal closed. Generate concepts in batches, run Hunyuan3D, retopologise and rig in Blender. Produce a folder of finished FBX and PNG files. Batching matters because Chroma takes minutes per image — queue ten and walk away rather than watching one.

Mode B Build days — Unreal only

ComfyUI closed. Import what Mode A produced, write C++, light, tune, playtest. This is where most days go, and where the game actually gets made.

Mode C Sound sessions — anywhere

Recording foley on a phone and building MetaSound graphs is light on the machine and can fill the gaps. Genuinely the best return per hour in the project, and the most neglected.


## So, concretely, next

- You: open the 5 migrated projects in 5.8 and confirm they look right

- You: restart Claude Code so both MCP servers load

- Grab the free Fab packs: Derelict Corridor (262 assets) + Dark Ruins

- Block out one corridor in UE5 Modeling Mode — no external tools

- First-person character in C++: walk, crouch, lean, torch

- Light it with Lumen. Establish the look

- First MetaSound ambience: sea, wind, drips, structural groan

- Measure stat unit. Write the number down. That is the baseline everything else is judged against

That is M1 — a corridor you are afraid to walk down. It is small on purpose. When it is finished it will have proved the art pipeline, the lighting look, the audio approach and the performance budget all at once, and everything after it becomes repetition rather than invention.

Back to The Build Log Every session recorded — the decisions, the engineering, and everything that broke on the way. ←

SEAWALL · workstation & pipeline Session 01 · 5 Sep 2026

==============================================================================
# PAGE: Session 02 - Surface and Light  (handbook.html)
==============================================================================

Seawall · Reference

# SURFACE & LIGHT

How the corridor's material system actually works — every node, why it exists, and what each of the 42 parameters does. Then the lighting model, so the horror can be tuned without guessing.

Master material1

Instances3

Parameters42

Textures used0


## Part one — the material


### The problem this solves

Every wall, floor and ceiling in the corridor is the same 100 cm cube, scaled. A wall scaled 18× has its UVs stretched 18× too — so any normal texture would smear horribly along the long walls and look correct on the short ones. There is no single texture scale that works.

The trick the whole material rests on

Map by world position instead of UVs. Every pattern is computed from where a point is in the world, not where it sits on a mesh. A wall scaled 18× and a wall scaled 3× get identical surface density, automatically. It also means patterns run continuously across separate actors — the tile grid crosses six separate floor boxes without a seam, because none of them know they are separate.


### The five nodes that do everything

The whole 42-parameter system is built from five node types repeated. Learn these and the graph stops being mysterious.

| Node | Does | Used for |
|---|---|---|
| WorldPosition | Where am I in the world? Outputs XYZ, XY and Z separately | The input to almost everything. Z drives the dado height and the mould/moss gates |
| Noise | Random but continuous values, 0–1, scattered through space | Every organic pattern. Scale = feature size, Levels = octaves of fine detail |
| SmoothStep | The threshold gate. See below | Turning smooth noise into distinct patches |
| Lerp | Blend between A and B by a mask | Every layer sitting on top of the last |
| Multiply | Combine masks, darken values | Stacking conditions: "corner and ceiling and patch" |


### SmoothStep — the one to actually understand

```
SmoothStep(Min, Max, Value)

  Value below Min   →  0     effect OFF here
  Value above Max   →  1     effect FULLY ON here
  in between        →  smooth fade
```

Feed it noise and you get patches. Then two moves control everything:

| Move | Result | Why |
|---|---|---|
| Raise Min | Fewer patches | Higher bar — only the strongest noise peaks clear it |
| Lower Min | More patches | Almost everything qualifies |
| Narrow the Min→Max gap | Hard, torn edges | The fade happens over a tiny range |
| Widen the gap | Soft, faded edges | The fade is spread out |

Every ...Start / ...End pair in the parameter list is one of these. PeelStart/PeelEnd, MouldPatchStart/End, MossPatchStart/End, StreakStart/End — same node, same two rules, different noise feeding it.


### The layer stack

Layers paint over each other bottom-up, exactly like real decay accumulates.

1 — DadoThe two-tone paint line

WorldPosition.Z feeds a SmoothStep between DadoHeightLow and High, then Lerps DadoColor (teal) into UpperColor (bone). Because it reads world height, the paint line sits at the same height on every wall no matter how the box is scaled — and it falls out for free that the floor (Z≈0) takes the teal and the ceiling (Z≈410) takes the bone.

2 — PeelingPaint torn away to concrete

Two noises at different scales are multiplied before the SmoothStep. One noise alone gives round repeating blobs; multiplying two gives irregular tears at genuinely different sizes. Underneath, exposed concrete is not one flat colour — ConcreteColor and ConcreteColorB are mixed by a medium noise to simulate aggregate, then pitted by a fine one.

3 — Water stainingVertical runs

The trick: world position is multiplied by (1, 1, 0.06) before sampling the noise. Squashing the Z axis stretches the noise vertically, turning round blobs into long streaks. Cheapest directional effect there is.

4 — Mould / bloodOne layer, two meanings

Height gate (near the floor) × large-scale patch gate × a detail noise. On MI_Wall it is dark green mould. On MI_Floor the identical layer is recoloured dark red and becomes blood pools. Same machine, different paint — no extra nodes.

5 — SaltSea air through concrete

White crystalline bloom near the floor. Specific to an island: salt drives into the structure and pushes back out. Free authenticity nothing inland would have.

6 — DetailTwo octaves of grain

Noise at ~8 cm and ~1.5 cm multiplied together. Without this the surface reads as soft low-resolution blobs, because the largest features were 1.7 m across and there was nothing at close range.

8 — TilesGrid from world position

frac(worldXY / TileSize) gives position inside each tile. Then f×(1−f) per axis peaks mid-tile and hits zero at the seams; multiplying the two axes together gives the grout mask without needing a Min node. Per-tile brightness variation comes from noise sampled at floor(uv) — constant within a tile, different between tiles.

9 — MossCorner-seeking growth

Four masks multiplied: corner proximity (abs(worldY) against a half-width), a height gate, a sparse patch gate, and a dense interior made from six octaves of turbulence raised to a Power. The power curve is what clumps it into cratered knots instead of a smooth wash.


### The principle worth taking to every material

One mask, every channel

A mask is not "a colour" — it is a region. Once built, feed it into everything that should change there. Blood is not just red: it is red and glossier. Exposed concrete is not just grey: it is grey and rougher than the paint. Moss is green and dead matte.

Roughness is therefore a four-stage chain, in the same order as the colour:

```
1. paint      Lerp(RoughnessMin, RoughnessMax, grime) × micro-noise
2. concrete   ...overridden by ConcreteRoughness where paint has peeled
3. blood/mould ...overridden by MouldRoughness on top of that
4. moss/grout ...overridden again where those masks apply
```

Blood sitting on exposed concrete takes the blood's finish, not the concrete's — because it is physically the topmost thing at that point.


## Master material vs instances

|  | M_Concrete_Master | MI_Wall / MI_Floor / MI_Ceiling |
|---|---|---|
| Holds | The logic — the node graph | Only parameter values |
| Editing costs | A full shader recompile — slow | Nothing. Applies live as you drag |
| Edit when | Adding a new kind of effect | Everything else |

This is why one material can produce three completely different surfaces. Rebuilding the master does not lose instance tuning — overrides are stored by parameter name, so as long as names are kept, values survive.

Applying them needed a workaround: neither overrideMaterials nor staticMesh is settable on an existing component in this build, so each surface role gets its own blockout mesh asset (SM_Blockout_Wall / _Floor / _Ceiling) carrying its instance.


## Three bugs that cost real time

| Symptom | Cause | Fix |
|---|---|---|
| Mirror floor, black colours, per-pixel stipple — and lighting that would not change | set_properties takes values as a JSON string, not an object. Passing an object returned true and wrote nothing | json.dumps() the dict. Found by setting a light to 1234 and reading back 8 |
| Power node refused to connect | Its inputs are Base / Exp | Not "Exponent" |
| AppendVector rejected a constant | It has no constB | Wire a real Constant node |

The lesson

A tool returning success is not evidence it worked. Read the value back. Hours were spent tuning lights that were never changing.


## Part two — lighting


### The four things that make a room dark

Darkness is not one setting. Four systems each brighten a scene, and any one left wrong keeps it lit.

| # | System | Where | Now |
|---|---|---|---|
| 1 | Exposure | PP_Corridor → Exposure Compensation | 4.5 |
| 2 | Surface albedo | The material's colours | ~0.05–0.2 |
| 3 | Ambient | SkyLight intensity | 0.05 |
| 4 | Lights | PL_Corridor_01/02/03 | 2200 lm |

The one most people miss

Auto-exposure. Left on, Unreal watches the screen and brightens whatever it sees — so every dark room is quietly lifted back to mid-grey and nothing you do to the lights survives. It is locked to manual here on purpose. If the corridor ever refuses to go dark, check this first.


### Light units

```
Intensity is in lumens when inverse-squared falloff is on.

    1700 lm  =  a 100W bulb
    2200 lm  =  our corridor lamps -- bright, but tiny reach
     400 lm  =  a dying emergency light
```

Attenuation Radius is the more important dial. It is a hard cutoff, in centimetres. Ours is 560 cm on lamps roughly 900 cm apart — so the pools do not touch, and genuine darkness sits between them. That gap is the horror. A corridor lit evenly end to end is a corridor with nowhere to hide.


### Colour temperature is free tension

Lamps are warm amber (255, 168, 96) against teal-green walls — near-opposites on the colour wheel. Warm pool, cold shadow. That contrast does more for atmosphere than any amount of extra geometry, and it costs nothing.


### Volumetric fog

Fog is what makes light visible in the air rather than only on surfaces — the shaft under a lamp, the way a distant light glows before you reach it.

| On | Setting | Now | Note |
|---|---|---|---|
| ExponentialHeightFog | Fog Density | 0.035 | Interiors want tiny values. 0.6 whited the corridor out completely |
|  | Fog Height Falloff | 0.04 | Kept low so fog is even, not pooled at the floor |
|  | Volumetric Fog | on | Without this, fog is a flat screen filter |
| Each light | Volumetric Scattering Intensity | 2.2 | How much that light shows in the air |

The gotcha that wasted a pass

The fog actor started at z = −6850. With height falloff, density decays exponentially with height — so at corridor level there was effectively zero fog and no setting had any visible effect. Move the fog actor to your play space before touching its numbers.


### Shadows are the expensive part

Every shadow-casting light is a full extra render pass. In a dark game the temptation is dozens of small lights — that is exactly how a horror project ends at 22fps. Keep very few casting shadows: the torch, and the one or two lamps whose shadows tell the player something.


### Where to start tuning

- Exposure Compensation on PP_Corridor — the master brightness. Everything else is relative to it

- Attenuation Radius before Intensity — controls where darkness lives, which matters more than how bright the light is

- One light at a time. Solo a lamp by dropping the others to 0, get it right, then bring them back

- Light Color — a sickly green or sodium orange changes the whole read of a space in one drag

- Fog Density last — it interacts with everything, so tune it once the lights are settled

The rule for horror lighting

Light a few things brightly and leave everything else genuinely black. The instinct is to add lights until you can see — resist it. The player should be straining to make out shapes at the edge of a pool of light. What they cannot quite see is doing the work.

Back to The Build Log The running record of every session — decisions, engineering, and what broke. ←

SEAWALL · surface & light Session 02 · 6 Sep 2026

==============================================================================
# PAGE: Session 03 - From Prompt to Fixture  (fixtures.html)
==============================================================================

Seawall · Session 03

# FROM PROMPT TO FIXTURE

An AI image becomes a sculpt, a sculpt becomes a game asset, and a game asset becomes a light that actually lights the corridor. Every step, including the four that broke — and the arithmetic that explains why a “working” emissive material rendered as dead grey plastic.

Generated7

Shipped2

Tris after cleanup35k / 40k

Baked maps8


## Part one — making the models


### The argument I lost, and should have

I pushed back on using ComfyUI for hard-surface props. The reasoning was reasonable and the conclusion was wrong: image-to-3D is bad at precise hard surfaces — screw threads, panel gaps, clean bevels — but a fifty-year-old light fitting rotting on an abandoned island is not a precise hard surface. It is a dented, corroded, half-collapsed one. That is exactly what these models are good at.

Lesson

“This tool is weak at hard-surface” was a real limitation applied to the wrong object. The asset we needed was defined by its damage, and damage is the thing generative models produce most convincingly. Check that a general rule actually applies to the specific case before leaning on it.


### The pipeline

Flux renders a product-shot concept, Hunyuan3D turns that single image into a mesh. The prompt conditions matter more than the subject description — every generation used the same frame:

```
a single old ceiling-mounted fluorescent light fixture, ...
three quarter view, whole object fully visible,
plain flat light grey background, soft even studio lighting,
product photograph, sharp focus
```

That second half is doing the work. Image-to-3D reconstructs what it can see, so a flat grey background with no cast shadows and the whole silhouette in frame is the difference between a clean mesh and a puddle. The negative prompt bans the things that quietly ruin it — collage, grid, multiple objects, cropped, dramatic lighting, hard shadows.


### What you actually get back

Seven models, ~80 seconds each. Then the reality check:

```
glTF version 2   size 1907 KB
meshes: 1     materials: 0 []     images/textures: 0 / 0
prim attrs: ['POSITION']
TRIANGLES: 107,840   VERTS: 53,322
```

POSITION and nothing else. No UVs, no normals, no materials — a raw sculpt. It is not an asset, it is the starting point for one. Everything below is the work of turning it into something Unreal can use.


## Part two — the Blender pass

1


### Orientation, and a silent no-op

Fixed

The ceiling panel imported standing on its end. Setting a rotation and applying it did nothing at all — twice — and the script reported success both times.

```
dim_before: [0.79, 0.245, 1.966]
dim_after:  [0.79, 0.245, 1.966]   rotated: true
```

The cause: the glTF importer sets rotation_mode = 'QUATERNION'. Assigning obj.rotation_euler on a quaternion object is ignored without error. The fix is to stop asking the object nicely and transform the mesh data directly:

```
obj.data.transform(obj.matrix_world)      # bake existing transform
obj.matrix_world = Matrix.Identity(4)
obj.data.transform(Matrix.Rotation(rz,4,'Z')
                 @ Matrix.Rotation(rx,4,'X'))
dim_after: [1.966, 0.79, 0.245]
```

Lesson

Same shape as the set_properties bug from Session 01: an API that accepts your input, returns success, and changes nothing. Read the value back. Twice now this has been the thing that caught it.

2


### Splitting the glass from the housing

Solved

A light needs two materials — dark rusted metal, and a lens that emits. The mesh arrives as one undifferentiated blob, so the lens faces have to be identified geometrically. The obvious approach is a positional test: faces pointing down, inside an inset rectangle. It half-works and looks terrible:

```
boundary tears straight through triangles
— ragged, stepped edge along the whole rim
```

Because a threshold on position knows nothing about the shape. The fix is to let the geometry define its own boundary: start from one face in the middle of the lens and grow outward across shared edges, refusing to cross any edge where the two faces meet at a sharp angle. The rim is a sharp angle, so the fill stops there exactly.

```
for e in f.edges:
    if e.calc_face_angle() > radians(22):
        continue        # sharp edge = the rim, do not cross
    ...grow into e.link_faces
```

Why this is the right tool

It is the same idea as “select linked flat” in the Blender UI, done in script. A positional test asks where is this face; a flood fill asks is this face part of the same surface. For anything bounded by a crease, the second question is the one that gives clean results.

3


### Pivots that make placement free

Done

A prop's origin decides how painful it is to place. Both fixtures got their pivot moved onto the surface that touches the building:

| Asset | Pivot | Result in Unreal |
|---|---|---|
| Ceiling panel | centre of the top face | Set Z = ceiling height. Done. No nudging. |
| Corner lamp | centre of the back plate | Set X = wall position. Sits flush. |

Verified after import by reading the bounds back out of Unreal, not by looking at it:

```
panel  z: -24.51 → 0.000006   # top face exactly at origin
lamp   x: 0 → 76.90          # back plate exactly at origin
```

4


### Weathering, and why it had to be baked

Done

The housings are painted steel that has been losing an argument with sea air for fifty years. Built procedurally: a low-frequency noise decides where corrosion takes hold, a higher-frequency noise decides its texture inside those regions, and Pointiness exposes bare metal along worn edges.

The multiply that made it look real

First attempt applied rust everywhere the fine noise fired — a uniform orange coat that read as plastic. Multiplying the rust mask by a much larger-scale region mask (scale 1.15) means corrosion appears in patches, with original paint surviving between them. Real decay is patchy; uniform decay reads as a texture, not a history.

None of which survives export. FBX cannot carry Blender's node graph — it carries geometry, UVs and material slots. Ship the FBX alone and Unreal shows grey. So every procedural layer gets baked down to flat image maps: BaseColor, Roughness, Normal, Emissive, at 2048×2048.

5


### The normal map that baked to nothing

Fixed

The bake reported success and produced a file that looked, at thumbnail size, like a normal map should — flat lavender. Measuring it instead of trusting it:

```
Panel_01  R min 119 max 139 sd 0.37
Panel_01  G min 118 max 136 sd 0.29
```

A neutral normal is 128. Deviating by less than half a value out of 255 is not surface detail, it is rounding noise. The bump feeding the bake was far too weak to register. After raising strength to 1.0 and distance to 0.42, and giving it a higher-contrast height source:

```
Panel_01  R min 22 max 235 sd 36.36
Lamp_01   R min 23 max 232 sd 27.44
```

Lesson

“It looks about right” is not verification for a data texture. A flat normal map and a subtle normal map are visually identical at a glance and completely different in engine. Two numbers — min/max range and standard deviation — settle it in seconds.


## Part three — into Unreal


### The import trap nobody warns you about

Unreal guesses texture settings from the filename. It got the normal maps right and both data masks wrong:

| Texture | Unreal chose | Correct |
|---|---|---|
| _Normal | TC_Normalmap, sRGB off | Right |
| _Roughness | TC_Default, sRGB on | TC_Masks, sRGB off |
| _Emissive | TC_Default, sRGB on | TC_Masks, sRGB off |

Why sRGB on a roughness map is wrong

sRGB is a curve for colour — it encodes brightness the way an eye perceives it. Roughness is not a colour, it is a number. Decoding it through the sRGB curve bends every value: a stored 0.5 arrives at the shader as roughly 0.21. The surface silently becomes far glossier than authored, everywhere.


### Master material, then instances

Two masters, four instances. The masters hold the graph; the instances hold nothing but parameter values, so changing a value costs no shader recompile.

| Master | Parameters | Instances |
|---|---|---|
| M_Fixture_Housing | BaseColor, Roughness, Normal, Metallic | MI_Panel_Housing, MI_Lamp_Housing |
| M_Fixture_Glass | EmissiveColor, EmissiveIntensity, GlassBaseColor, GlassRoughness | MI_Panel_Glass, MI_Lamp_Glass |


## Part four — the light that would not glow

The panel was in the level, materials assigned, emissive wired, and it rendered as a slab of dead grey plastic. The graph checked out — MP_EmissiveColor was driven by Multiply(Multiply(mask, EmissiveColor), Intensity) exactly as authored. Nothing was broken. The number was simply wrong by a factor of sixty, and the reason is in the post-process volume:

```
autoExposureMethod: AEM_Manual
autoExposureBias: 3.9
autoExposureApplyPhysicalCameraExposure: true
cameraShutterSpeed: 60   cameraISO: 100   depthOfFieldFstop: 4
```

That last flag is the one that matters. With it on, the physical camera settings apply on top of the manual exposure. ISO 100 at 1/60s and f/4 is a bright-daylight exposure — roughly −10 EV — and the +3.9 bias only claws part of it back. Net, scene values reach the screen multiplied by about 0.0156.

```
emissive 7  × 0.0156  =  0.11 on screen   — dead grey
```

```
emissive 140 × 0.0156  =  2.18 on screen   — glows, blooms
```

There is no universal “correct” emissive value

An emissive of 7 is bright in one project and invisible in another. The number only means something relative to the exposure the scene is graded at. Before hunting for a bug in a material, read the post-process volume — the answer is usually there, and it is arithmetic rather than a fault.


### Why 420 glowed but came out white

Overshooting produced a light that worked and had lost all its warmth. At intensity 420 with a warm tint, every channel clears the tonemapper's white point:

```
420 × (1.00, 0.78, 0.48) × 0.0156  =  (6.5, 5.1, 3.1)
all three > 1  →  clips to pure white
```

```
140 × (1.00, 0.74, 0.40) × 0.0156  =  (2.18, 1.62, 0.87)
blue stays under 1  →  warm core, warm bloom
```

Colour survives only where at least one channel is below clip. Push every channel past white and hue is mathematically gone, no matter what tint is set.

The mask that was doing nothing

The glass material originally multiplied by a baked emissive mask. But the mask marks the lens faces — and those faces already have their own material slot. The slot is the mask. It was removed: one less texture sample, one less failure mode, and the whole lens now emits evenly, which is what a diffuser does anyway.


## Part five — placement and the room lights

A fixture is not a light

The emissive mesh makes the lamp look lit. It contributes almost nothing to lighting the space. The Rect Light beside it does that job. Both are needed, and they must agree: when the panels went in flush to the ceiling, the existing Rect Lights at z 377–383 ended up inside the panel geometry, where the mesh would shadow its own light. They moved to 373.5, just below the emitting face.


### Dimming a room properly

“Too bright” is usually three separate problems, and intensity only fixes one of them. A wide, far-reaching soft source stays everywhere however dim you make it:

| Dial | Went | What it controls |
|---|---|---|
| Intensity | 1900 → 620 | Raw brightness in candelas. The blunt instrument. |
| Attenuation Radius | 700 → 430 | How far light reaches before being forced to zero. At 700 the rooms were leaking into the corridor. |
| Barn Door Angle | 88° → 42° | Rect-light only. 88° is nearly a full hemisphere — the panel sprayed every wall. 42° pools it under the fixture. |
| Barn Door Length | 20 → 22 | How far the flaps extend; shapes how hard that edge falls off. |

Reach for Intensity when it is simply too bright, Attenuation Radius when light goes where it should not, and Barn Door Angle when the walls and corners are lit but you wanted a pool. Never the post-process exposure — that is global, and drags the whole corridor with it.


## Where things stand

- 7 fixtures generated; 2 taken through to shippable assets

- Geometry cleaned, decimated, UV'd, pivots on the mounting faces

- Procedural weathering baked to 8 texture maps, verified numerically

- Imported with scale, orientation and pivots confirmed against Unreal's own bounds

- Two master materials, four instances, all dials exposed

- 7 panels and 1 corner lamp placed; 16 blockout placeholders retired

- Corridor and room lighting rebuilt around the real fixtures, warm-tinted

- The wall/room tube role is unfilled — that model was cut

- No performance baseline yet: stat unit has still never been run

- Fab sign-in still pending — scanned Megascans normals remain the biggest quality gap

The thread running through this session

Four separate bugs — the silent rotation, the flat normal bake, the sRGB masks, the dead emissive — and every one of them reported success. Not one would have been caught by looking at the result and nodding. They were caught by reading a value back and comparing it to what was asked for. That is the whole discipline.

Related Surface & Light The corridor's material system — every node, every parameter, and how the lighting model works. →

SEAWALL · from prompt to fixture Session 03 · 7 Sep 2026

==============================================================================
# PAGE: Session 04 - The Body You Cannot See  (character.html)
==============================================================================

Seawall · Session 04

# THE BODY YOU CANNOT SEE

A first-person character with weight, built without a single frame of animation. Why Arthur Morgan's heaviness lives in the camera rather than the skeleton, what we deliberately refused to build, and the crouch bug that put the player's eyes at ankle height.

C++ classes2

Animations0

Content assets needed0

MilestoneM1


## The question that framed everything

The brief was “can we mimic Arthur Morgan — that weight, that sense that a body takes time to move.” The instinct is that this needs a character: a skeleton, animations, foot placement. It does not, and understanding why saved the entire session.

Rockstar solved a different problem

You watch Arthur. Third-person weight has to be visible, so it needs a motion-matched skeleton with feet that plant correctly. You do not watch your own character — you are them. In first person the same weight is delivered by two things that have no skeleton at all: how the movement accelerates, and what the camera does about it.

“Whole character” versus “just the camera” turned out to be a false choice. There are three layers and only one is optional:

| Layer | What it is | Optional? |
|---|---|---|
| Capsule & movement | The physical body. Collides, fits through doors, crouches under things | Never. Same work either way |
| Camera feel | Bob, sway, landing dip, lean, FOV shifts, breathing | No — in first person this is the weight |
| Visible mesh | Arms, hands, legs, body | Yes. The only part deferred |

So a whole character got built. Just not a character model.


## The numbers, and what each one is for

| Setting | Value | Why |
|---|---|---|
| Walk | 150 cm/s | Deliberate. Exploration should carry dread, not commute |
| Crouch | 75 cm/s | Slow enough that hiding is a real cost |
| Sprint | 420 cm/s | Genuinely fast, and genuinely brief |
| Max acceleration | 900 | The weight number. ~0.45s to reach walk speed, so you lean into motion rather than teleporting into it |
| Braking deceleration | 1100 | You slide a few centimetres past where you released. Nothing in a body stops dead |
| FOV | 75 → 81 sprinting | Narrow vision means things enter frame suddenly. The FOV push is most of what sells a sprint |
| Stamina | 100, −25/s | Four seconds of sprint. A panic button with a price, not transport |

Head bob is the most overdone effect in first-person games

Too much causes motion sickness and reads as amateur — RDR2's own first-person mode is far more restrained than people remember. The target is “you would notice if it were gone, but you never notice it is there.” So it ships with a master on/off switch, because the only way to find that line is to walk the corridor with it on, then off.

The bob is driven by distance travelled, not elapsed time, so footsteps stay in sync whether you are creeping or sprinting. Time-driven bob drifts out of step the moment speed changes, and it always looks wrong without anyone being able to say why.


## Three decisions made now to avoid pain later

1


### The camera hangs off a body that does not exist yet

Hook

FirstPersonBody is an empty USkeletalMeshComponent, and the camera is attached to it rather than to the capsule. Pointless today. At M3, when a MetaHuman arrives, it becomes a mesh assignment instead of a restructure — HeadSocketName and HeadSocketDamping are already waiting.

Why damping is already in the design

A true first-person body drives the camera from the head bone. Piping raw head-bone motion straight into the view is precisely what makes players sick, so the camera must follow the head with damping rather than being welded to it. Designing that in from the start costs one float. Retrofitting it means rewriting the camera.

2


### Footsteps already shout into an empty room

Wired

Every footstep reports a hearing event — walk 400, sprint 1400, crouch 120, landing 900 units. Nothing listens. Nothing will until M2.

```
UAISense_Hearing::ReportNoiseEvent(
    GetWorld(), GetActorLocation(), Loudness, this, Range, TEXT("Footstep"));
```

Lesson

The whole horror loop rests on the creature hearing you. Adding that after movement exists means auditing every code path that can move a player — walking, sprinting, landing, being pushed, ledge grabs that do not exist yet. Adding it while there is exactly one path costs four lines.

3


### Input that needs no content authored

Done

Enhanced Input normally wants Input Action and Mapping Context assets created by hand before a single key works. Instead the character builds them in code when none are assigned — so it ran the moment it compiled, with zero content. Every action is still an exposed property, so assigning real assets later makes them rebindable without touching code.


## The bug that put the player's eyes at ankle height

Standing worked. Crouching would have dropped the view to roughly 54 cm off the floor — not crouched, lying down. Nobody had crouched yet, so nobody had seen it.

The cause is a mismatch between two systems that both look correct alone. The camera sits at a fixed offset from FirstPersonBody, which is placed at the capsule's base for a standing capsule. But when Unreal crouches a character it shrinks the capsule and drops the actor so the feet stay planted — and that fixed offset no longer points at the floor.

```
standing:  capsule centre 92 above floor, body at −92, eye +174 → 174 ✓
crouched:  capsule centre 50 above floor, body at −92, eye +98  → 56 ✗
```

```
const float HalfHeightCompensation =
    GetDefaultHalfHeight() - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
// crouched: 92 - 50 = 42, added back → eye lands at 98 above the floor
```

The body offset is now derived from the capsule rather than hard-coded, so the two cannot drift apart again.

Lesson

Two correct-looking systems can be wrong together. A constant copied from one place into another is a bug waiting for someone to change the original — derive it instead.


## Height, and the wrong thing to blame

First walkthrough verdict: “it looks like our main character is a dwarf.” The eye sat at 165 cm — a 175 cm person. Entirely normal.

The corridor is the problem, not the character

The ceiling is at 4 metres. Real corridors are 2.4–3 m. Under a ceiling a third taller than reality, any human reads as short. The character went to 184 cm because it was asked for, but the honest fix is dropping the ceiling — which would also make the corridor more oppressive, which is the point of the whole level.


## Jump: forgiveness, not height

Jump strength varies by what you were doing — standing 340, walking 385, sprinting 445, with sprint leaps costing double stamina and getting a little more air control so they can be aimed. But the strength is not what makes a jump feel good.

| Mechanic | Window | What it fixes |
|---|---|---|
| Coyote time | 0.12s | A jump pressed just after walking off a ledge still counts. Players press late; this forgives it |
| Input buffering | 0.16s | A jump pressed just before landing fires on touchdown instead of being swallowed |
| Variable height | ×0.45 on release | Tap for a hop, hold for the full arc. One button, two intentions |

Why this matters more than the numbers

Without these three, roughly one press in five appears to be ignored — and it genuinely was ignored, correctly, by a system that only accepts input during the exact frames the player is grounded. Players do not experience that as their own mistiming. They experience it as the game being unresponsive. All three mechanics exist to accept input the player meant rather than input they precisely delivered.


## What was deliberately not built

- Arms and hands — only visible while holding something, and nothing exists to hold. The rig depends entirely on whether it is a flashlight, a lighter or a crowbar. Build now, build twice

- A flashlight model — it is a bare spot light on the camera. Correct for a prototype

- A true first-person body — wanted as a feature, deferred to M3, and built from MetaHuman rather than by hand. Legs that do not quite match where your feet are break immersion worse than seeing nothing


## Where things stand

- First-person character with weighted movement, stamina, lean, crouch and jump

- Camera feel: distance-driven bob, landing dip, exhaustion breathing, sprint FOV push

- Flashlight with battery wired but drain at zero

- Footstep hearing events broadcasting, ready for M2

- Game mode in C++, PlayerStart in the corridor, Play works

- The corridor has been walked. This was the whole point

- No audio at all — a horror corridor in silence cannot really be judged

- stat unit still never run. No performance baseline exists

- Nothing hunts you yet. That is M2, and it is the actual game

The session in one line

A pretty corridor is a screenshot. A corridor you can walk down badly is a prototype. The difference is entirely in numbers nobody will ever see.

Previously From Prompt to Fixture An AI image becomes a sculpt, a sculpt becomes a game asset, and a light finally glows. ←

SEAWALL · the body you cannot see Session 04 · 7 Sep 2026

==============================================================================
# PAGE: Session 05 - The Light That Was Never On  (lumen.html)
==============================================================================

Seawall · Session 05

# THE LIGHT THAT WAS NEVER ON

A week away, a system drive with 2.3 GB left, and a routine performance check that exposed the biggest mistake in the project so far: global illumination had been silently switched off since the day the project was created — while two whole sessions of lighting were tuned on top of it.

Days Lumen was off9

Warnings logged0

fps without GI189

fps with Lumen123


## Part one — a drive with 2.3 GB left

First check after a week away: C: was 100% full. It had been 13 GB free when we stopped. That matters more than it sounds — Unreal constantly writes shaders, caches and saves, and a system drive that runs out mid-write is how projects get corrupted. Nothing got launched until that was fixed.

| What | Size | Verdict |
|---|---|---|
| pip download cache | 16.8 GB | Cleared. Six ~2.5 GB PyTorch installers from late 2025. Pure cache |
| uv download cache | 19 GB | Pure cache, but in use by running MCP servers — clean later |
| Downloads folder | 44 GB | Personal files. Not ours to touch |
| hiberfil.sys | 12.7 GB | Optional: powercfg /h off. Costs Fast Startup |
| NVIDIA DXCache | 5.6 GB | Leave it. The driver's shader cache — clearing makes every game stutter |
| Visual Studio installer leftovers | 4.0 GB | Unpacked install files from the toolchain setup. Probably safe |

What a package cache actually is

When Python's installers (pip, uv) download a package, they keep a copy so the next install is instant. Neither ever deletes those copies. PyTorch builds with GPU support are about 2.5 GB each, so every project that ever installed one left one behind. The cache was cleared with pip's own command, pip cache purge, rather than deleting the folder by hand — and only after pip confirmed that exact folder was its cache.

Look before deleting

The hibernation file looked like free space, but another project on this machine (a remote-wake tool) involved sleep states. Its notes showed it used S3 sleep, which never touches the hibernation file, and the wake feature had been dropped anyway — so it is safe. Five minutes of checking beats breaking something that lives in a different folder.


## Part two — measuring instead of guessing

The plan was a quick performance baseline: open the console, type stat unit, read the numbers. That shows one moment. Instead we used Unreal's built-in CSV profiler, which records exact timings for every frame into a file — averages, the slowest 1% of frames, spikes, and a permanent record to compare every future change against.

| Number | What it measures |
|---|---|
| FrameTime | Total time for one frame. 16.7 ms = 60 fps |
| GameThreadTime | Gameplay code — movement, the character, logic |
| RenderThreadTime | CPU work deciding what to draw |
| RHIThreadTime | CPU work handing that to the graphics driver |
| GPUTime | The graphics card actually drawing it |

Every condition of the capture was chosen so the number would mean something:

- Editor closed — its own viewport also renders the corridor, and would steal GPU time

- Standalone game at a fixed 1920×1080 — closer to a shipped build, and repeatable

- VSync off — otherwise every frame waits for the monitor and everything reads a flat 16.7 ms

- First 600 frames thrown away — they include loading and shader warmup, not gameplay

1


### The capture that “hung” for seven minutes

Fixed

The profiler never produced a file. After seven minutes the game was force-closed and the investigation started — crash reports, the Windows event log, the engine source. Then the real log turned up:

```
LogCsvProfiler: Capture Ended. Writing CSV to file :
  .../Users/Intel/AppData/Local/UnrealEngine/5.8/Saved/Profiling/CSV/...
LogCsvProfiler:   Frames : 2400
```

It finished 23 seconds after launch. When the editor program runs a project with -game, it writes logs and profiling data to the engine's user folder, not the project's Saved folder. The game ran perfectly the whole time while we watched an empty folder.

Lesson

“It hung” and “I am looking in the wrong place” look identical from the outside. Before assuming something failed, confirm where it actually writes.


## Part three — the pass that was not there

The baseline came back excellent: 189 fps average, only 32% of the 60 fps budget. But the profiler also lists every GPU pass — each separate job the graphics card does per frame — and something was missing from it.

What global illumination is

Light bouncing. A lamp lights the floor; the floor throws some of that light back up onto the walls and ceiling. Without it, anything a light does not hit directly gets only flat ambient fill, plus a cheap darkening effect in corners (SSAO). In a dark concrete corridor, bounce is most of what makes light feel real. Lumen is Unreal's system for calculating it in real time.

The project was configured for Lumen. Yet among 26 active GPU passes, not one belonged to Lumen. Instead the list contained SSAO and screen-space reflections — exactly the older, cheaper effects Lumen replaces when it runs. The capture's own metadata said raytracing = 0. And both the standalone game and the editor had logged this:

```
LogRendererCore: Ray tracing is disabled.
  Reason: disabled through project setting (r.RayTracing=0).
```


### Why it switched itself off without a word

Lumen has two ways to work out where light bounces. It can use the graphics card's dedicated ray-tracing hardware, or it can trace against distance fields — a simplified 3D copy of every mesh, handled in software. The engine source is explicit that it needs one of them:

```
bool Lumen::IsSoftwareRayTracingSupported()
{
    return DoesProjectSupportDistanceFields();
}
...
&& (Lumen::UseHardwareRayTracing(*View.Family) || IsSoftwareRayTracingSupported());
```

If neither is available, Lumen quietly declines to render and Unreal falls back to SSAO. No error. No warning. The only trace is a line about ray tracing being disabled, buried in a thousand-line startup log.


### How it happened

The project config written in Session 01 (commit 794077f) turned on r.Lumen.HardwareRayTracing=1 — “use hardware ray tracing for Lumen”. But git history shows the project never contained either thing underneath it: no r.RayTracing to allow hardware ray tracing at all, and no r.GenerateMeshDistanceFields for the software path. Even Epic's stock First Person template ships with both. A hand-written config dropped them.

The lesson of the whole session

A setting that says a feature is on is not evidence the feature is on. The config read correctly, the corridor looked plausible, and nothing complained. It took a performance measurement — listing what the GPU actually did — to notice. Everything lit in Sessions 02 and 03 was tuned without global illumination.


## Part four — switching it on properly

| Option | Trade-off |  |
|---|---|---|
| Hardware ray tracing | Best quality the RTX card can give; glowing panels light the walls around them. Costs a shader recompile and a re-tune | Chosen |
| Software Lumen | Cheaper, but the corridor is built from heavily stretched cubes, and distance fields handle stretched meshes badly |  |
| No GI | Free, keeps the approved look — but the island at M4 becomes far harder to light |  |

What a shader model is

Shaders are the small programs the graphics card runs to draw every pixel. They are compiled for a particular shader model — roughly, a version of the language the card speaks. The engine defaults to SM5. Hardware ray tracing needs SM6. The card itself supports 6.7; the project simply never asked for it, so the engine never even checked whether ray tracing was possible.

The fix was copied from Epic's own 5.8 templates rather than written from memory — memory is how the gap appeared:

```
[/Script/Engine.RendererSettings]
r.RayTracing=True
r.SkinCache.CompileShaders=True

[/Script/WindowsTargetPlatform.WindowsTargetSettings]
-D3D12TargetedShaderFormats=PCD3D_SM5
+D3D12TargetedShaderFormats=PCD3D_SM6
```

The hidden dependency: skin cache

In the engine source, turning on ray tracing through Project Settings pops up “Ray Tracing requires enabling skin cache” — and answering no switches ray tracing back off. Editing the config file skips that prompt entirely, so it has to be set by hand. It is also what lets animated characters, like the Watchman at M2, appear in ray-traced lighting at all.

SM6 replaces SM5 rather than being added beside it, which would compile every shader twice. Distance fields were deliberately left off for now — they only matter as a fallback for players without ray-tracing cards, which is a shipping concern for M9.

Verified from the log before trusting it:

```
LogD3D12RHI: Creating D3D12 RHI with Max Feature Level SM6
LogRendererCore: Ray tracing is enabled (dynamic). Reason: r.RayTracing=1
LogRendererCore: Ray tracing shaders are enabled.
```

The full shader recompile ran on 8 worker processes and took about 4 minutes. The warning beforehand said 30–60 — an estimate that was wrong, in the good direction.


## Part five — proof, not a setting

A setting saying ray tracing was on is exactly what fooled everyone before. So the identical capture was run again — same resolution, same view, editor closed:

| GPU pass | Before | After |
|---|---|---|
| SSAO | 0.088 ms | gone |
| Screen-space reflections | 0.023 ms | gone |
| Lumen reflections | — | 0.186 ms |
| Lumen scene update | — | 0.095 ms |
| Ray tracing scene | — | 0.076 ms |
| Measure | No GI | Lumen HWRT | Change |
| Frame time (avg) | 5.30 ms | 8.14 ms | +2.84 |
| Game thread | 1.84 ms | 2.08 ms | +0.24 |
| RHI thread | 1.62 ms | 3.35 ms | +1.73 |
| GPU | 4.94 ms | 6.92 ms | +1.98 |
| Average fps | 189 | 123 |  |
| 1% low fps | 157 | 89 |  |

Most of the added cost is the CPU rebuilding the ray-tracing scene every frame (the RHI thread, +1.73 ms) and about 2 ms more GPU work. The average frame still uses only about half of the 60 fps budget.


### The number that did not add up

One pass still looked wrong. LumenScreenProbeGather — the part that computes bounced light, normally one of Lumen's most expensive jobs — measured 0.005 ms. Either bounce was not really working and only reflections were, or the profiler was filing its cost under another heading (the Lights pass had tripled, 0.46 → 1.55 ms).

Rather than keep digging, it got settled by eye. In the running game, the console command r.Lumen.DiffuseIndirect.Allow 0 switches bounce off, and 1 switches it back. With bounce off, walls beside a pool of light go flat; with it on, warm light creeps up them and the ceiling around each panel glows. The difference was visible — bounce is working. Why that one timer reads so low is still unexplained, and is recorded as such in the baseline notes.

Numbers and eyes, both

The profiler proved Lumen was running. Only looking proved the bounce was doing something. Neither alone would have been enough — which is the same lesson as the whole session, from the other side.


## Where things stand

- C: back from 2.3 GB to 18 GB free; the disk pattern is recorded so it gets checked each session

- Two profiled baselines with notes in Profiling/Baselines: without GI, and with Lumen

- Hardware ray tracing, SM6 and skin cache enabled, verified in both editor and game logs

- Lumen proven active by the passes the GPU runs; bounce light confirmed by eye

- The corridor lighting needs re-tuning — every light was set without bounce, so it will read brighter now

- Optional cleanup: uv cache 19 GB, hibernation file 12.7 GB, installer leftovers 4 GB

- Distance-field fallback for GPUs without ray tracing — M9

- Next: the first sound pass

The session in one line

Two sessions of lighting were built on a renderer quietly ignoring the main thing it had been asked for — and it took a performance test, not a look at the screen, to find out.

Previously The Body You Cannot See A first-person character with weight, built without a single frame of animation. ←

SEAWALL · the light that was never on Session 05 · 14 Sep 2026
