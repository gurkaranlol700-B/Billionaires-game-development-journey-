# How I Built Seawall

A running journal of the actual work — what we did, why, and the things that broke.
Written so that if you came back in six months with no memory of any of it, you could
rebuild the whole thing from this file.

---

## Day 1 — Setting up the ground

### What we're making

**SEAWALL** — a first-person survival horror game set on Kurogane-jima, a fictional
abandoned concrete mining island off Nagasaki. 4–5 hours. Scarce combat. You're looking
for your missing father, and your friend Kaito is going to trade you for his sister.

Full design lives in the plan file at
`C:\Users\Intel\.claude\plans\yo-bro-so-we-shimmering-feather.md`.

---

### The machine, and why it matters

| Part | What you have | What it means for us |
|---|---|---|
| GPU | RTX 5060 Ti 16GB | Lumen + Nanite + hardware ray tracing + DLSS 4. Plenty for 1080p60. |
| CPU | i5-14400F, 10 cores / 16 threads | Shader compiles are CPU-bound. This is fine, not fast. |
| RAM | 32 GB | **The weak link.** UE5 likes 48–64. Close ComfyUI before opening Unreal. |
| Disk | One 2TB NVMe SSD, partitioned C/D/F/G | D: is the *same physical drive* as C:, so putting things there costs zero speed. |

**The disk situation was the first real problem.** C: had only 18 GB free. Unreal projects
generate enormous amounts of derived data — shader caches, cooked content, intermediate
build files — and would have filled that in a week. So everything Unreal now lives on D:,
which has 358 GB.

That's also why the earlier UE 5.8 install died: the Epic Launcher defaulted to C:, ran out
of room, and left behind a dead `.egstore\Pending` marker folder.

---

### Migrating the old projects

You had 5 projects in `C:\Users\Intel\Documents\Unreal Projects`:
`car_game_5hrs`, `landscape_1`, `masterclass_game`, `my_first_unreal_game`, `real_npc_car_ai`.

**The key discovery: only 1.14 GB of their 7.81 GB was real.**

This is worth understanding, because it's true of every Unreal project you will ever make:

| Folder | Keep? | What it is |
|---|---|---|
| `Content/` | **YES** | Your actual assets — meshes, materials, Blueprints, levels |
| `Config/` | **YES** | Project settings (`.ini` files) |
| `Source/` | **YES** | C++ code, if the project has any |
| `Binaries/` | no | Compiled output. Rebuilt on demand. |
| `Intermediate/` | no | Build scratch space. Rebuilt on demand. |
| `DerivedDataCache/` | no | Cached shaders/textures. Regenerated on demand. |
| `Saved/` | no* | Logs, autosaves, screenshots, packaged builds |

\* Check `Saved/SaveGames` and `Saved/Screenshots` before deleting — those *are* yours.
We checked; both were empty on all five projects.

So the migration was: copy those three folders plus the `.uproject`, and let Unreal
regenerate everything else. Copied 1.2 GB, verified file counts matched exactly, and left
the C: originals in place as a fallback until you've confirmed all five open in 5.8.

**Why copy and not move:** upgrading a Blueprint project from 5.7 to 5.8 is a *one-way*
conversion. If something breaks, you want the original still sitting there. All 5 projects
are Blueprint-only (no `Source/` folder), which makes this upgrade about as low-risk as
Unreal upgrades get — no code to recompile, just asset format conversion.

---

### The toolchain fight (the useful part)

Unreal C++ doesn't compile with "Visual Studio installed." It needs a specific set of pieces,
and yours was missing one. **We found this on day one on purpose**, with a throwaway project,
rather than in week three with real work on the line.

**What Unreal actually needs to build C++:**

1. **An MSVC compiler** in a version Unreal accepts
2. **A Windows SDK** in range
3. **The .NET Framework SDK** — for `SwarmInterface`, a legacy build-distribution module
4. **.NET 8 runtime** — to run UnrealBuildTool itself (Unreal bundles its own, so this was fine)

Unreal writes down its exact requirements in a readable file — worth knowing this exists:

```
<Engine>\Engine\Config\Windows\Windows_SDK.json
```

It lists `MinimumVisualCppVersion`, a `BannedVisualCppVersions` list (versions with known
codegen bugs!), and `PreferredVisualCppVersions`.

**Problem 1 — a warning, not a blocker.** You had MSVC 14.50. That's above the 14.38 minimum
and not banned, but outside the preferred 14.44 range, so UBT printed:

```
Visual Studio 2026 compiler version 14.50.35725 is not a preferred version.
Please use the latest preferred version 14.44.35207
```

Harmless, but we installed 14.44 alongside it so we're on the toolchain Epic actually tests.
Both versions coexist; UBT picks the preferred one.

**Problem 2 — the actual blocker.** The build died with:

```
Unable to instantiate module 'SwarmInterface': Could not find NetFxSDK install dir
Result: Failed (RulesError)
```

Your VS 2026 had the C++ compiler but **no .NET Framework SDK at all** — the
`C:\Program Files (x86)\Windows Kits\NETFXSDK` directory didn't exist. Until that was fixed,
*no* Unreal C++ project could build on this machine.

**The subtle bit that cost us a round trip:** ".NET Framework targeting pack" and ".NET
Framework SDK" are *different components*. I installed the targeting pack first
(`Microsoft.Net.Component.4.6.2.TargetingPack`) and it landed in `Reference Assemblies\` —
but UBT doesn't look there. Reading UBT's own source settled it:

```csharp
// MicrosoftPlatformSDK.cs
string[] PreferredVersions = new string[] { "4.6.2", "4.6.1", "4.6" };
string NetFxSDKKeyName = "Microsoft\\Microsoft SDKs\\NETFXSDK";
TryReadInstallDirRegistryKey32(NetFxSDKKeyName + "\\" + PreferredVersion, "KitsInstallationFolder", ...)
```

It reads a **registry key**, which only the SDK component creates. The fix was
`Microsoft.Net.Component.4.6.2.SDK` — `.SDK`, not `.TargetingPack`.

**Lesson worth keeping:** when a build tool says it can't find something, find where the tool
*looks*. Unreal ships its full C# source under `Engine\Source\Programs\UnrealBuildTool`, so
you can always just read it.

**Installing VS components from the command line** (faster than clicking through the GUI):

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe' `
  modify --installPath "C:\Program Files\Microsoft Visual Studio\18\Community" `
  --add Microsoft.Net.Component.4.6.2.SDK --passive --norestart
```

One gotcha that bit us: PowerShell's `-ArgumentList` as an **array** silently split
`C:\Program Files\...` at the space, so the installer saw `--installPath C:\Program`, found no
product there, and exited 1 without doing anything. Pass the arguments as a **single string**
with embedded quotes instead.

**Verified working.** Rebuilt the throwaway project after the fix:

```
Using Visual Studio 2026 14.44.35223 toolchain
  (C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207)
  and Windows 10.0.26100.0 SDK

[3/7] Compile [x64] ToolchainTest.cpp
[6/7] Link    [x64] UnrealEditor-ToolchainTest.dll
Result: Succeeded          Total execution time: 66.90 seconds
```

Note it picked 14.44 by itself and the "not a preferred version" warning is gone — UBT
prefers the tested toolchain when one is available, so installing it was worth doing.

**Keep `D:\UnrealProjects\ToolchainTest`.** It's a 6-file project that compiles in ~60s, which
makes it the fastest possible way to answer "is the engine/compiler setup healthy?" We'll
point it at 5.8 and rebuild as the first smoke test after the upgrade.

**How to re-run it any time:**

```bash
cd /d/UnrealProjects/ToolchainTest
"/c/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" \
  ToolchainTestEditor Win64 Development \
  -Project="D:\UnrealProjects\ToolchainTest\ToolchainTest.uproject" -WaitMutex
```

---

### Source control, set up before it's needed

`git init` plus **Git LFS configured before the first asset was committed.**

**Why this ordering is non-negotiable:** `.uasset` and `.umap` files are binary. Git stores a
whole new copy of a binary file on every change — so a 200 MB level edited fifty times becomes
10 GB of unusable history. LFS keeps them outside the repo and stores a small pointer instead.
Adding LFS *after* you've committed binaries means rewriting history, which is genuinely
painful. So `.gitattributes` exists from commit zero.

---

### The MCP setup — how I actually work inside your editor

Two servers, doing different jobs:

**1. Epic's built-in Unreal MCP** (shipped in UE 5.8, June 2026 — this is why we upgraded).
It embeds an MCP server *inside the editor process*, so an AI agent can spawn actors,
configure lighting, make material instances, and run automation tests.

```
Plugins to enable:  Unreal MCP, All Toolsets  (Toolset Registry comes along automatically)
Editor Preferences > General > Model Context Protocol > Auto Start Server
Console command:    ModelContextProtocol.GenerateClientConfig ClaudeCode
```

That writes a `.mcp.json` into the project root. It binds `http://127.0.0.1:8000/mcp`,
**loopback only, no authentication, experimental** — fine locally, never expose that port.

**2. Our own `seawall-mcp`** at `Tools/seawall-mcp/server.py`. Epic's server drives the
*editor*; it can't do the things I need most while writing C++:

| Tool | What it's for |
|---|---|
| `project_info()` | Sanity check — engine path, modules, whether it's ever been built |
| `build()` | Compile and return **only the error lines** (raw build logs are ~50k lines of noise) |
| `tail_log()` | Read `Saved/Logs/Seawall.log` — where every `UE_LOG` and Print String lands |
| `find_in_log()` | Regex search the log with context |
| `run_tests()` | Headless automation tests |
| `latest_screenshot()` | Let me actually see the game |

Same architecture as your `blender-mcp`: a PEP 723 header at the top of the file declares its
own dependencies, so `uv run server.py` installs `mcp` by itself — no venv to manage. The
difference is this one doesn't use a socket at all; it just runs UnrealBuildTool and reads
files, which is much harder to break.

---

---

### The Epic Launcher install that couldn't be fixed by freeing space

The 5.8 install kept failing with a dialog saying *"Install Failed — try to free up space on
your drive and attempt to install again"*, error code **`IS-IN-DS01`**.

`DS` is Epic's code for **disk space**, and the launcher log confirmed it:

```
LogEoshPatcher: BpsEndStats.ErrorCode: 'DS01', TaskError: 'Tsk-Bps:UE-Bps:UE-DS01'
LogDownloadManager: HandleTaskComplete: AppId [ue:...:UE_5.8] AlertCode=[IS-IN-DS01]
                    QueueLen=3 IncompleteInstall=1
LogAutoUpdateService: The installation failed ... the installer will no longer try to
                      perform an automatic update again until the user specifies it
```

**But freeing space would not have fixed it**, and this is the useful lesson. The launcher had
written a *pending install record* that hard-pinned the destination:

```
C:\ProgramData\Epic\EpicGamesLauncher\Data\Manifests\Pending\<hash>.item

  InstallLocation       C:\Program Files\Epic Games\UE_5.8    <-- pinned
  StagingLocation       C:\Program Files\Epic Games\UE_5.8\.egstore/bps
  InstallSize           31693779023   (31.7 GB)
  bIsIncompleteInstall  True
```

Because a half-finished install existed, the Library showed **Resume** instead of a fresh
install dialog — so there was **no drive picker to change**. Every retry went back to C: and
failed the same way. The log even shows it refusing outright:

```
AppState=[AppPatching] not installable, reporting InstallFailed (II-E1003) + cancelling
```

**The fix** was to delete the stuck record so the launcher would offer a clean install:

1. Close the launcher (it holds these files open)
2. Delete `...\Manifests\Pending\<hash>.item`
3. Delete the `C:\Program Files\Epic Games\UE_5.8` stub
   — verified first that it contained exactly one file, a 35 MB `.manifest`, and **zero
   engine binaries**, so nothing real was lost
4. Restart the launcher

**Where useful state lives, worth remembering:**

| Path | What it holds |
|---|---|
| `C:\ProgramData\Epic\EpicGamesLauncher\Data\Manifests\*.item` | One JSON per installed/pending app — install path, version, size |
| `...\Manifests\Pending\` | Queued or half-finished installs (**the thing that gets stuck**) |
| `C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat` | What the launcher believes is installed |
| `%LOCALAPPDATA%\EpicGamesLauncher\Saved\Logs\EpicGamesLauncher.log` | The actual error codes |

**How it ended:** 5.7 was uninstalled first (reclaiming 26.6 GB, taking C: from 16 GB to 42 GB
free), and 5.8 then installed to C: — 42 − 31.7 leaves about 10 GB. Tight, which is why the
DDC redirect below matters.

---

### Keeping the derived-data cache off C:

Unreal generates a **derived data cache** — compiled shaders, converted textures — that
routinely reaches 10–50 GB. By default it goes to
`%LOCALAPPDATA%\UnrealEngine\Common\DerivedDataCache`, i.e. the C: drive, which we do not have
room for.

Redirected before the engine ever ran, so nothing had to be migrated:

```powershell
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', 'D:\UnrealDDC\Local', 'User')
```

Unreal reads that environment variable at startup. Combined with all projects living on D:,
the only thing left on C: is the engine itself.

---

## Open items

- [x] Copy 5 projects to `D:\UnrealProjects\` — verified by file count **and** byte size
- [x] Fix the C++ toolchain (.NET Framework 4.6.2 SDK + MSVC 14.44) — verified by real compile
- [x] Write `seawall-mcp` server
- [x] git + LFS configured before any binary asset
- [x] Clear the stuck Epic install record
- [x] Uninstall UE 5.7 (reclaimed 26.6 GB)
- [x] Redirect DDC to `D:\UnrealDDC\Local`
- [ ] UE 5.8.2 finishing its install (31.7 GB → `C:\Program Files\Epic Games\UE_5.8`)
- [ ] Re-run the `ToolchainTest` smoke test against 5.8
- [ ] Verify all 5 migrated projects open and run in 5.8 **(your job — you know what they should look like)**
- [ ] Delete the C: project originals → reclaims another 7.9 GB
- [ ] Create the Seawall project, wire both MCPs
- [ ] **M1: a corridor you're afraid to walk down**
