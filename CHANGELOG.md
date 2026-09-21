# Changelog

All notable changes to Z80 Explorer are documented in this file. For details, always see "Z80Explorer" main document.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)

## [Unreleased]
### Added
- Built-in MCP server so LLM clients can drive the simulator: load HEX, run cycles, read nets, sample waveform windows, walk equation trees, trace fanout, control the watchlist, and rename or delete net names
- Settings dialog with runtime-configurable waveform history depth and server enable/port options
- Filter search box above the net lists in the edit dialogs
- Drag-and-drop of any customization JSON, several files at once, with CTRL to merge dropped colors
- Drag-and-drop of .asm sources onto the Sim Monitor, assembled by the bundled zmac
- Pull-up transistor symbol detection and overlay in the image view
- test_every_op and test_every_pla_bucket diagnostic programs
- GitHub Actions release workflow building Windows, macOS and Linux artifacts

### Improved
- resource/ reorganized into purpose-named subfolders: chip/, user/, scripts/, zx/, tests/
- Waveform: movable divider, persistent horizontal scrollbars, bus value text that follows the visible left edge, and double-click on a watchlist row to edit it
- Edit Colors: explicit Save button, persistent file path, and refined Enabled toggle behaviour across selected rows
- Find centers the image view on the located feature and pans to it along an easing curve
- Wheel zoom behaves the same on Windows and macOS
- zmac is bundled for macOS and Linux as well as Windows

### Fixed
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
