# Z80Explorer
Visual Zilog Z80 netlist-level simulator

[![Release](https://img.shields.io/github/v/release/gdevic/Z80Explorer)](https://github.com/gdevic/Z80Explorer/releases)
[![Build](https://github.com/gdevic/Z80Explorer/actions/workflows/release.yml/badge.svg)](https://github.com/gdevic/Z80Explorer/actions/workflows/release.yml)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-lightgrey)](LICENSE)

Z80 Explorer is a Zilog Z80 netlist-level simulator capable of running Z80 machine code and an educational tool with features that help reverse engineer and understand this chip better.

## Features

- Cycle-accurate simulation of all 8,881 transistors and 3,597 nets in the original Z80 die
- Layered die visualization (diffusion, polysilicon, metal, vias, buried contacts, ions) with click-to-select on any net or transistor
- Waveform capture and replay for arbitrary nets, with cursors, bus decoding, and a circular history buffer
- JavaScript scripting through QJSEngine (`loadHex`, `run`, `reset`, `eq`, …) plus drag-and-drop `.js` execution
- Built-in MCP HTTP server exposing 30+ tools so LLM clients (Claude Code, etc.) can drive the simulator
- Intel HEX program loading and ZX Spectrum 48K SNA snapshot support; `zexall` passes

Read more in the blog here: [Blog](https://baltazarstudios.com/)
<br>
Application's User's Guide: [Users Guide](https://gdevic.github.io/Z80Explorer)
<br>
It is also described in a blog: https://baltazarstudios.com/z80explorer
<br>
A brief overview is on YouTube: https://youtu.be/_dyngzTEnvw
<br>
The annotated overview is on Vimeo: https://vimeo.com/439468449

## Quick start

Launch the app, drag-and-drop `resource/tests/hello_world.hex` onto the main window (or run `loadHex("hello_world.hex")` in the *Command* dock), then `reset()` followed by `run(0)`. The waveform dock shows live signal traces; clicking any feature on the die identifies its net and current logic state.

Read the [User's Guide](https://gdevic.github.io/Z80Explorer).<br>

![Z80 Explorer](https://baltazarstudios.com/wp-content/uploads/2020/07/z80explorer-app.png)

## Pre-built binaries

Download from the [GitHub releases page](https://github.com/gdevic/Z80Explorer/releases). Builds are available for Windows, macOS, and Linux (glibc 2.35+).

On Linux, you may need the XCB cursor library:

    sudo apt install libxcb-cursor0

## Building from source

Requires Qt 6.9+ and a C++17 compiler. Either build system works: `Z80Explorer.pro` (qmake) or `CMakeLists.txt` (CMake). The simplest path is to open one of these in Qt Creator and pick the *Release* kit.

**Windows** — Visual Studio 2022 (MSVC, x64) plus the matching Qt kit. AVX2 is enabled automatically.

**macOS** — Xcode command-line tools (`xcode-select --install`) plus the macOS Qt kit. AVX2 paths are auto-disabled on Apple Silicon.

**Linux** — gcc or clang, Qt's *Desktop gcc 64-bit* component, and the dev libraries below:

    sudo apt install build-essential libgl1-mesa-dev libxcb-xinerama0 libxcb-cursor0

## Running test programs

Pre-assembled HEX files live in `resource/tests/`. Drag any of them onto the running app, or load via JS: `loadHex("zexall.hex"); reset(); run(0)`. 

## MCP server (drive the simulator from Claude Code)

Z80Explorer ships a built-in [MCP](https://modelcontextprotocol.io) server so an LLM client can drive the simulator: load HEX, run cycles, read nets, sample waveform windows, walk equation trees, and so on.

Enable it in *Edit → Settings…*, tick **Enable MCP server**, and (optionally) change the port from the default `8765`. The server binds to `127.0.0.1` only — the client must run on the same machine, or you need an SSH tunnel. Confirm it's up:

    curl http://localhost:8765/mcp/healthz

Register it with the Claude Code CLI once (stored in `~/.claude.json`, available across all your projects):

    claude mcp add --transport http --scope user z80explorer http://localhost:8765/mcp

List the available tools with `claude mcp list`, or run `/mcp` inside a Claude Code session to see the live connection state.

## Compiling zmac assember on Linux

If you are going to compile and run Z80 test programs, you need zmac assembler. Download it from here: http://48k.ca/zmac.html<br>
I am not aware of a prebuilt binary for Linux, but it is fairly easy to build since the source is available at that site. You may also need:
* sudo apt-get install bison

## Credits

The transistor and segment definitions originate with the [Visual 6502 team's Z80 reverse-engineering project](http://www.visual6502.org/JSSim/expert-z80.html). This simulator builds on their netlist data with a Qt 6 UI, performance optimizations, scriptable controls, and an MCP surface. Test programs are assembled with [zmac](http://48k.ca/zmac.html).

## License

Creative Commons BY-NC-SA 4.0 — see [`LICENSE`](LICENSE).
