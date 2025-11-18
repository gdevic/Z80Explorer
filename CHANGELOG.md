# Changelog

All notable changes to Z80 Explorer are documented in this file. For details, always see "Z80Explorer" main document.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)

## [Unreleased]

### Improved
- Massive performance boost of ~2x
  - On i7-13700K @5.3GHz the simulation runs at 10.3kHz

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

[unreleased] https://github.com/gdevic/Z80Explorer/compare/v.107...HEAD \
[1.07] https://github.com/gdevic/Z80Explorer/compare/v.106...v.107
