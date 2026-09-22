# Changelog

All notable changes to Z80 Explorer are documented in this file. For details, always see "Z80Explorer" main document.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)

## [Unreleased]
### Added
- Built-in MCP server so LLM clients can drive the simulator: load HEX, run cycles, read nets, sample waveform windows, walk equation trees, trace fanout, control the watchlist, and rename or delete net names
- MCP results carry `structuredContent`, so a client gets validated JSON instead of parsing it back out of a text block. Five tools declare an `outputSchema` to go with it.
- Every MCP tool publishes behaviour hints and a display title, so a host can tell the 15 read-only tools apart from the ones that move the simulation or reach outside it.
- MCP resources for the static chip data: the named-net table, the chip pins, and per-net templates for details, drivers, fanout and logic equation.
- MCP prompts for the investigations this project repeats: trace a net's drivers, characterise a PLA row, capture an opcode's timing, find the latch behind a signal.
- MCP argument completion over net names, so a model completes a name instead of guessing it.
- Settings dialog with runtime-configurable waveform history depth and server enable/port options
- Filter search box above the net lists in the edit dialogs
- Drag-and-drop of any customization JSON, several files at once, with CTRL to merge dropped colors
- Drag-and-drop of .asm sources onto the Sim Monitor, assembled by the bundled zmac
- Pull-up transistor symbol detection and overlay in the image view
- test_every_op and test_every_pla_bucket diagnostic programs
- GitHub Actions release workflow building Windows, macOS and Linux artifacts
- File > Save User Data... (Ctrl+S) writes the user data files at any point in a session instead of only when the application closes. Any subset or all of them, from one dialog listing each item with its target path. Saving does not interrupt a running simulation.
- The same registry from scripts, the command socket and MCP: `save()`, `save("id")` and `saveList()` in JavaScript, and the `z80_save` tool over MCP
- An item may refuse to write and say why, rather than destroy what is on disk: an empty waveform view keeps its saved configuration, and colors merged but not committed keep theirs across an exit. Results separate what was written from what was skipped and what failed.
- A save that fails on the way out now interrupts the exit instead of losing the session silently

### Changed
- `save()` in the Command dock used to save by emitting the application shutdown signal, which also stopped the simulation and the script engine. It now writes the files and leaves the session alone. `shutdown()` means only that the application is quitting.
- `saveNetnames()` now writes the per-net comments (tips.json) along with netnames.js, so the two files can no longer drift apart
- Waveform *Save As...* and *Load...* now adopt the file you pick, and the choice survives a restart. Previously the window went on writing `waveform-N.json` regardless, so work done in a loaded file landed somewhere else on close. *Merge...* still keeps the current file, since a merge is a blend of sources rather than a move. This matches what Edit Colors already did.
- MCP server now speaks protocol revision 2026-07-28 and only that revision. The revision is stateless: there is no initialize handshake, every request carries its own protocol version and client capabilities, and a client discovers the server with `server/discover`. A client that opens with the old handshake is told which revision to use instead of being left guessing.

### Improved
- resource/ reorganized into purpose-named subfolders: chip/, user/, scripts/, zx/, tests/
- Waveform: movable divider, persistent horizontal scrollbars, bus value text that follows the visible left edge, and double-click on a watchlist row to edit it
- Edit Colors: explicit Save button, persistent file path, and refined Enabled toggle behaviour across selected rows
- Find centers the image view on the located feature and pans to it along an easing curve
- Wheel zoom behaves the same on Windows and macOS
- zmac is bundled for macOS and Linux as well as Windows
- User's Guide brought up to date with the code

### Fixed
- `z80_run` missed the completion signal for runs of one or two half-cycles, so single-stepping waited out the full timeout and then reported `timeout`. It now returns as soon as the run ends.
- `z80_run` with `timeout_ms` of zero started no timer and could block the application indefinitely. The timeout is now clamped.
- `z80_run` returned success after a timeout while leaving the simulation running, so every value it reported, and every later read, came from a netlist still being rewritten. It now halts the chip first.
- The pin tools silently did nothing when given the upper-case pin names their own documentation advertised, and still reported success. Pin names are now case-insensitive and an unknown pin is a real error.
- Tool failures were reported as successful results. They now set `isError`, and an unknown net names the closest existing nets instead of saying only "unknown net".
- Out-of-range net ids were read straight out of the netlist arrays. Net arguments are now range-checked.
- The MCP endpoint accepted any `Origin` and any content type, which left the JavaScript escape-hatch tool reachable from a web page through DNS rebinding. Cross-origin requests are refused and a JSON content type is required.
- `z80_net_read` declared its entries as objects while the handler wanted names or numbers, and the no-argument tools declared a schema that accepted anything.
- Black-square holes and per-segment stroke seams in net rendering
- QWidget warning on clean exit; the servers now stop while qApp is still alive

### Removed
- Qt IFW-based installer
- Offscreen renderer; z80_view_grab routes through the Export PNG dialog instead

## [1.09] - 2026-01-06
### Added
- CMake build system and Qt IFW installer
- AVX2-optimized netlist simulator

### Improved
- ***The code is now using AVX-2 instructions for the additional 30% perf gain!***
- Requires CPU with AVX2 support (Intel Haswell 2013+, AMD Excavator 2015+)
- More performance optimizations (cached net lookups, QSet duplicate detection)
- Faster startup (concurrent image loading, segment merging)
- Waveform rendering

### Fixed
- Build compatibility with gcc 15 and clang

## [1.08] - 2025-11-18
### Improved
- Massive performance boost of ~2x
  - On i7-13700K @5.3GHz the simulation runs at 10.5kHz

## [1.07] - 2025-11-09
### Added
- New implementation of the schematic code
- Added Schematic Edit dialog
- Multiple ways to draw nets
  - Active (legacy view), Pull-up (static), Gate-less (static) and Gate-less no Pull-up (static)
- More Waveform draving options
  - Added fill for logic 0 or for logic 1
  - Added "Sync" button that syncs waveform views' cursors
  - Added Merge menu and Save option
- Added patchHex() command
- Added execApp() command
- Load and run ZX Spectrum SNA format: load("sna.js")
- Added socket server, port 12345, and sample sock.py client (disabled at the moment)

### Improved
- Schematic engine
- Save and restore waveform cursors across sessions
- Better Colors management
- Added autocompleter to Find
- Added background checkered pattern
- Added context menu to toggle overlay
- Added context menu to save image view as PNG
- Waveform add vertical scroll, Δ= and Sync buttons
- Simulation engine performance improvements
- Edit Buses allows extended selection for Delete
- Updated resources
  - Added names of all registers, updated netnames etc.
  - Added waveform-2 with PLA signals
- Documentation

### Fixed
- More robust loading of nets and buses
- Make ZX window single, not closable

[unreleased] https://github.com/gdevic/Z80Explorer/compare/v.109...HEAD
[1.09] https://github.com/gdevic/Z80Explorer/compare/v.108...v.109
[1.08] https://github.com/gdevic/Z80Explorer/compare/v.107...v.108
[1.07] https://github.com/gdevic/Z80Explorer/compare/v.106...v.107
