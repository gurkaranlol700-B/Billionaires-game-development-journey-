"""
Decode the newest Unreal MCP CaptureViewport result into a PNG.

CaptureViewport returns the screenshot as base64 inside JSON, which is far too
large to come back through the tool channel, so the harness spills it to a file
under .claude/.../tool-results/. This grabs the newest such file, pulls the
image out, and writes it somewhere readable.

    python Tools/shot.py [output.png]
"""

import base64
import glob
import json
import os
import sys

RESULTS = os.path.expandvars(
    r"%USERPROFILE%\.claude\projects\c--Users-Intel-Desktop-claude"
    r"\406ca823-5a2a-430d-a9bb-a46373b7db32\tool-results\mcp-unreal-call_tool-*.txt"
)
DEFAULT_OUT = os.path.expandvars(
    r"%LOCALAPPDATA%\Temp\claude\c--Users-Intel-Desktop-claude"
    r"\406ca823-5a2a-430d-a9bb-a46373b7db32\scratchpad\viewport.png"
)


def main() -> int:
    out_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUT

    files = glob.glob(RESULTS)
    if not files:
        print("no tool-result files found")
        return 1
    newest = max(files, key=os.path.getmtime)

    raw = open(newest, encoding="utf-8", errors="replace").read()
    start = raw.find("{")
    if start < 0:
        print(f"no JSON in {newest}")
        return 1

    data = json.loads(raw[start:])
    rv = data.get("returnValue", data)

    # CaptureViewport nests the image; CaptureEditorImage / CaptureAssetImage
    # return the ToolsetImage directly.
    img = rv.get("image", rv)
    if "data" not in img:
        print(f"no image payload in {newest}")
        return 1

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as fh:
        fh.write(base64.b64decode(img["data"]))

    kb = os.path.getsize(out_path) // 1024
    print(f"wrote {out_path}  ({kb} KB)")

    if "cameraLocation" in rv:
        loc, rot = rv["cameraLocation"], rv["cameraRotation"]
        print(
            f"camera  loc=({loc['x']:.0f}, {loc['y']:.0f}, {loc['z']:.0f})"
            f"  rot=(pitch {rot['pitch']:.0f}, yaw {rot['yaw']:.0f})"
            f"  fov={rv.get('cameraFOV', 0):.0f}"
        )
    if rv.get("labeledActors"):
        print(f"labeled actors: {len(rv['labeledActors'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
