# /// script
# requires-python = ">=3.10"
# dependencies = ["mcp>=1.2.0"]
# ///
#
# NOTE ON THE MCP SDK VERSION
# ---------------------------
# mcp 2.x renamed FastMCP to MCPServer and moved it from mcp.server.fastmcp to
# mcp.server.mcpserver. The import below tries the new name first and falls
# back to the old one, so this file runs on either version. The alternative --
# pinning "mcp<2" -- would work today but leaves us stuck on a dead API.
"""
Seawall MCP server
==================

Epic's built-in "Unreal MCP" plugin drives the *editor* - spawning actors,
making materials, poking at Slate widgets. Great, but it cannot do the four
things I need most while writing C++ for you:

    1. compile the project and hand me the actual compiler errors
    2. read the game log so I can debug what happened at runtime
    3. run automation tests
    4. look at screenshots the game produced

That is what this server is. It does not talk to a running editor at all - it
works on the filesystem and runs UnrealBuildTool directly, which makes it much
harder to break than a socket connection.

Same shape as your blender-mcp server: the PEP 723 header at the top means
`uv` installs the `mcp` dependency by itself, so there is no venv to manage.

Run it by hand (you normally will not - Claude Code launches it from .mcp.json):
    uv run server.py
"""

import os
import re
import subprocess
from pathlib import Path

try:
    # mcp 2.x
    from mcp.server.mcpserver import MCPServer as _Server, Image
except ImportError:  # pragma: no cover
    # mcp 1.x
    from mcp.server.fastmcp import FastMCP as _Server, Image

# ---------------------------------------------------------------------------
# Paths. This file lives at <Project>/Tools/seawall-mcp/server.py, so the
# project root is two directories up. Deriving it instead of hard-coding means
# the whole project folder can be moved or renamed without editing this file.
# ---------------------------------------------------------------------------
PROJECT_ROOT = Path(__file__).resolve().parents[2]
PROJECT_NAME = "Seawall"
UPROJECT = PROJECT_ROOT / f"{PROJECT_NAME}.uproject"

# Where the engine lives. Override with the SEAWALL_ENGINE environment variable
# if the engine is ever installed somewhere else. The fallback list is tried in
# order so this keeps working if the engine is later moved to D: to free up C:.
_ENGINE_CANDIDATES = [
    r"C:\Program Files\Epic Games\UE_5.8",
    r"D:\Epic Games\UE_5.8",
]


def _find_engine() -> Path:
    override = os.environ.get("SEAWALL_ENGINE")
    if override:
        return Path(override)
    for candidate in _ENGINE_CANDIDATES:
        if Path(candidate, "Engine", "Build", "BatchFiles", "Build.bat").exists():
            return Path(candidate)
    return Path(_ENGINE_CANDIDATES[0])


ENGINE_ROOT = _find_engine()
BUILD_BAT = ENGINE_ROOT / "Engine" / "Build" / "BatchFiles" / "Build.bat"
EDITOR_CMD = ENGINE_ROOT / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"

LOG_FILE = PROJECT_ROOT / "Saved" / "Logs" / f"{PROJECT_NAME}.log"
SCREENSHOT_DIR = PROJECT_ROOT / "Saved" / "Screenshots"

mcp = _Server("seawall")


def _fail(msg: str) -> str:
    """Consistent, human-readable error text rather than a stack trace."""
    return f"ERROR: {msg}"


@mcp.tool()
def project_info() -> str:
    """Report the state of the Seawall project: where it is, which engine it is
    pointed at, whether the engine and build tools were found, which C++ modules
    exist, and whether a compiled editor binary is currently present.

    Call this first when something is confusing - it catches "wrong engine path"
    and "never compiled" problems immediately."""
    lines = [
        f"Project root : {PROJECT_ROOT}",
        f"uproject     : {UPROJECT}  ({'found' if UPROJECT.exists() else 'MISSING'})",
        f"Engine root  : {ENGINE_ROOT}  ({'found' if ENGINE_ROOT.exists() else 'MISSING'})",
        f"Build.bat    : {'found' if BUILD_BAT.exists() else 'MISSING'}",
    ]

    src = PROJECT_ROOT / "Source"
    if src.exists():
        mods = sorted(p.name for p in src.iterdir() if p.is_dir())
        lines.append(f"C++ modules  : {', '.join(mods) if mods else '(none)'}")
    else:
        lines.append("C++ modules  : no Source/ directory yet")

    dll = PROJECT_ROOT / "Binaries" / "Win64" / f"UnrealEditor-{PROJECT_NAME}.dll"
    lines.append(f"Editor binary: {'built' if dll.exists() else 'NOT BUILT - run build() first'}")

    if LOG_FILE.exists():
        kb = LOG_FILE.stat().st_size / 1024
        lines.append(f"Log file     : {LOG_FILE.name} ({kb:.0f} KB)")
    else:
        lines.append("Log file     : none yet (game/editor has not run)")

    return "\n".join(lines)


@mcp.tool()
def build(target: str = "", configuration: str = "Development") -> str:
    """Compile the project with UnrealBuildTool and return the result.

    On failure this returns only the actual error lines rather than the whole
    build log, which is what makes it useful - the raw log is tens of thousands
    of lines and mostly noise.

    Args:
        target: Build target. Defaults to "<ProjectName>Editor", which is what
            you want while working in the editor. Use "<ProjectName>" for a
            standalone game build.
        configuration: "Development" (default, with debug info), "DebugGame"
            (slowest, fullest debug info) or "Shipping" (fastest, for release).
    """
    if not BUILD_BAT.exists():
        return _fail(
            f"Build.bat not found at {BUILD_BAT}. "
            "Is SEAWALL_ENGINE pointing at the right engine install?"
        )
    if not UPROJECT.exists():
        return _fail(f"{UPROJECT} not found.")

    target = target or f"{PROJECT_NAME}Editor"

    proc = subprocess.run(
        [
            str(BUILD_BAT), target, "Win64", configuration,
            f"-Project={UPROJECT}", "-WaitMutex",
        ],
        capture_output=True, text=True, errors="replace", timeout=3600,
    )
    out = proc.stdout + proc.stderr

    if proc.returncode == 0:
        # Surface warnings even on success - they are how you catch problems early.
        warns = [l for l in out.splitlines() if "warning" in l.lower()][:15]
        msg = f"BUILD SUCCEEDED ({target} | {configuration})"
        if warns:
            msg += "\n\nWarnings:\n" + "\n".join(warns)
        return msg

    # Failure: pull out the lines that actually say what went wrong.
    interesting = [
        l for l in out.splitlines()
        if re.search(r"\berror\b|\bfatal\b|cannot |could not |Unable to ", l, re.I)
    ]
    detail = "\n".join(interesting[:60]) if interesting else out[-4000:]
    return f"BUILD FAILED (exit {proc.returncode}) for {target} | {configuration}\n\n{detail}"


@mcp.tool()
def tail_log(lines: int = 120) -> str:
    """Return the last N lines of the game/editor log (Saved/Logs/Seawall.log).

    This is the single most useful debugging tool here: anything printed with
    UE_LOG in C++, or Print String in Blueprint, lands in this file. After the
    game does something unexpected, read this."""
    if not LOG_FILE.exists():
        return _fail(f"No log at {LOG_FILE}. Run the editor or game at least once.")
    text = LOG_FILE.read_text(encoding="utf-8", errors="replace")
    tail = text.splitlines()[-max(1, lines):]
    return "\n".join(tail)


@mcp.tool()
def find_in_log(pattern: str, context: int = 2, max_hits: int = 40) -> str:
    """Search the log for a regular expression and return matching lines with
    surrounding context.

    Use this instead of tail_log when you know what you are looking for - for
    example pattern="LogSeawall" for our own logging, or "Error|Warning" for
    problems.

    Args:
        pattern: Regular expression to search for (case-insensitive).
        context: Lines of context to show either side of each hit.
        max_hits: Stop after this many matches.
    """
    if not LOG_FILE.exists():
        return _fail(f"No log at {LOG_FILE}.")
    try:
        rx = re.compile(pattern, re.I)
    except re.error as e:
        return _fail(f"Bad regular expression: {e}")

    rows = LOG_FILE.read_text(encoding="utf-8", errors="replace").splitlines()
    chunks, hits = [], 0
    for i, row in enumerate(rows):
        if rx.search(row):
            lo, hi = max(0, i - context), min(len(rows), i + context + 1)
            chunks.append(f"--- line {i + 1} ---\n" + "\n".join(rows[lo:hi]))
            hits += 1
            if hits >= max_hits:
                break
    return "\n\n".join(chunks) if chunks else f"No matches for {pattern!r} in {LOG_FILE.name}."


@mcp.tool()
def run_tests(test_filter: str = "Seawall") -> str:
    """Run Unreal automation tests headlessly and report pass/fail.

    Args:
        test_filter: Which tests to run. Defaults to our own "Seawall" tests.
            Use "Project" for all project tests.
    """
    if not EDITOR_CMD.exists():
        return _fail(f"UnrealEditor-Cmd.exe not found at {EDITOR_CMD}.")

    proc = subprocess.run(
        [
            str(EDITOR_CMD), str(UPROJECT),
            f"-ExecCmds=Automation RunTests {test_filter}; Quit",
            "-unattended", "-nopause", "-nullrhi", "-nosplash",
        ],
        capture_output=True, text=True, errors="replace", timeout=1800,
    )
    out = proc.stdout + proc.stderr
    results = [
        l for l in out.splitlines()
        if re.search(r"Test (Completed|Started)|LogAutomation.*(Passed|Failed)", l, re.I)
    ]
    return "\n".join(results[-60:]) if results else f"No test output. Exit {proc.returncode}.\n{out[-2000:]}"


@mcp.tool()
def latest_screenshot() -> Image:
    """Return the most recent screenshot the game or editor saved, so I can see
    what the game actually looks like rather than guessing.

    In-game, the console command `HighResShot 1920x1080` writes one of these."""
    if not SCREENSHOT_DIR.exists():
        raise RuntimeError(f"No screenshot directory at {SCREENSHOT_DIR}.")
    shots = sorted(SCREENSHOT_DIR.rglob("*.png"), key=lambda p: p.stat().st_mtime)
    if not shots:
        raise RuntimeError("No screenshots found. Use `HighResShot 1920x1080` in the console.")
    return Image(path=str(shots[-1]))


if __name__ == "__main__":
    mcp.run()
