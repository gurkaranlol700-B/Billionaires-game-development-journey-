# Billionaires — Game Development Journey

Build journal and source for **SEAWALL**, a first-person survival horror game
built in Unreal Engine 5.8.

**Read the journal:** <https://billionaires-game-development-journ.vercel.app/>

## Setting

Kurogane-jima — a fictional abandoned concrete mining island off Nagasaki,
visually modelled on Gunkanjima. Your father vanished there four months ago.
Your friend Kaito is going to trade you for his sister.

4–5 hours. Scarce combat. Story-first.

## What's in here

| Path | What it is |
|---|---|
| `index.html` | The build journal — single file, no build step |
| `HOW-I-BUILT-THIS.md` | Same journal in markdown, with extra command-level detail |
| `Tools/seawall-mcp/` | MCP server — build, log tailing, tests, screenshots |
| `Source/` | C++ game code (arriving at M1) |
| `Content/` | Unreal assets, via Git LFS (arriving at M1) |

## Deploying the journal

Static site, no build step. On Vercel: import this repo, framework preset
**Other**, leave build command and output directory empty. It serves
`index.html` from the root.
