# TODO: Z80Explorer app improvements

Worklist for the app itself, collected from the 2026-09 verification rounds: what the agents hit, the "Proposed MCP / script API" sections of `How-Z80-Ticks/archive/audit_2026_09/reports/`, and a read of the current source. Defects already triaged live in `BUGS.md`. Priority is P1 (blocks or distorts research), P2 (costs a lot of time), P3 (convenience).

## 1. User data files: one treatment for all of them

The target for every user file: loaded at start from `resource/user/`, listed in the save registry (`ClassController::init()`), so it appears in File > Save User Data, `save()` / `saveList()`, `z80_save`, and the save on exit, and editable through a script call and an MCP tool that report success or the reason for failure.

### 1.1 `latches.ini` (P1)

Today it is read by `ClassVisual::loadLatches()` at start and by `relatch()`, and never written. Latches can only be added by editing the file by hand and calling `relatch()`, and nothing can list them or read their state.

- Add a `latches` save item. The writer keeps the file format (`t1,t2 ; name ; comment`), the `-` entries that suppress wrongly auto-detected latches, and the comment lines. Since the file is hand-edited, the simplest safe writer rewrites only the entry lines it owns and keeps every other line in place.
- Script: `latchList()` returning `{t1, t2, net1, net2, name, comment, value, source: "auto"|"ini"}`, `latchAdd(t1, t2, name, comment)`, `latchRemove(t1)` (writes a `-` entry for an auto-detected latch), and `relatch()` kept.
- MCP: `z80_latch_list` (with current values, filterable by name), `z80_latch_add`, `z80_latch_remove`.

### 1.2 `tips.json` and the comments in `netnames.js` (P1)

The same per-net comment is stored twice. `tips.json` loads first, then every trailing `// ...` in `netnames.js` overrides it, and the `netnames` save item writes both. There is no script or MCP call to set a tip; the only way in is dropping a JSON onto the die view. That drop also makes the dropped file the save target for the rest of the session (`ClassTip::getFileName()`), so the real `user/tips.json` is not written until the dropped file is copied back.

- Decide the single source of truth. Suggested: `netnames.js` holds the comment of every named net, and `tips.json` holds only comments for unnamed nets. The two can then never disagree.
- Script: `tipSet(net, text)`, `tipGet(net)`, `tipDelete(net)`.
- MCP: `z80_tip_set` / `z80_tip_get`, or a `comment` argument on `z80_rename_net` plus a `comment` field in `z80_net_info`.

### 1.3 Other files that need the same treatment

| File | Now | Needed |
|---|---|---|
| `user/schem.ini` | Read once to seed QSettings `schematicTermNodes`; edits in the dialog live only in the registry and are never written back, so the file and the app drift apart. | A `schematic` save item that writes the terminating-node list back, and a script/MCP call to read and set it. |
| `user/annot_functional.json`, `user/annot_internals.json` | Reachable only by dropping them; the drop makes the dropped file the annotations save target. | Load them as named annotation sets (or merge on load), each with its own save row. Script/MCP to add, list and delete annotations. |
| Buses (inside `netnames.js`) | `ClassNetlist::addBus()` exists but has no script or MCP entry. | `busAdd(name, nets)`, `busDelete(name)`, `busList()`; MCP equivalents. Saved by the existing `netnames` item. |
| `user/colors.json` | Save item exists; the path lives in QSettings `colorsFile` and a drop changes it. No script/MCP call. | Script/MCP to add or remove a color rule. |
| `user/waveform-N.json` | Save items exist; paths in QSettings. | Script/MCP to add or remove nets in a waveform view. |
| `user/pullups.json` | A regenerated cache written by `ClassVisual`. | Leave it out of the registry; move it out of `user/` (for example to a `cache/` folder) so `user/` holds only authored data. |
| `chip/nodenames.js` | Static and never written, but hand-edited (the `pla22` comment). Its names are overridden by `netnames.js`. | Document which file wins, or fold its user-editable part into `netnames.js`. |

### 1.4 Dropping a file changes where the app saves (P2)

Dropping a JSON onto the die view (`WidgetImageView::dropEvent`) makes that file the save target for annotations, colors, watchlist and tips. It surprised the workflow twice in this audit. Make the default action "import into the current `user/` file" and keep "open as the working file" as an explicit choice, for example Shift+drop. Say which one happened in the log.

## 2. Script API defects found during the audit (P1)

- **`run(n)` returns at once.** `ClassScript::run` calls `doRunsim`, which starts the worker thread, so `run(50)` followed by a read sees about 5 half-cycles. Scripts had to loop over `run(1)`. Add a blocking form (`runWait(n)`, or `run(n, true)`) that returns once the count is reached, and state in the description that `run` is asynchronous.
- **Pin timing does not take effect while stepping.** `set("_int",0)` and `setAt("_int",40,0)` left `_int` high while stepping with `run(1)`; the MCP `z80_pin_set_at` with `z80_run` works. Investigate.
- **The trickbox INT trigger from Z80 code did not arm.** Writes to D00Ah / D00Ch (`tb_int_pc`, `tb_int_hold`) left `readState()` at `int at: 0`. Investigate.
- **16-bit half-cycle arguments.** `stopAt`, `setAt` and `setAtPC` take `quint16` because the trickbox block is Z80-addressable. From a script or MCP call, a half-cycle above 65535 is truncated. Check the comparison against `curCycle` and either take 32-bit values in the host API or reject out-of-range ones.
- **Naming calls return nothing.** `setNetName`, `renameNet` and `deleteNetName` return `void`; a refusal (for example `renameNet` on an unnamed net) goes only to the log, so from a script it looks like nothing happened. Return `{ok, error}`, or merge them into one `nameNet(net, name, comment)` that names, renames or clears.
- **Two half-cycle numberings.** The JS tracer's `mon.getHCycle()` read before `run(1)` is the MCP canonical number plus one, so every report had to convert. Use one numbering everywhere, and name it in `z80_die_info`.
- **Reset does not clear every latch.** F and the register-select flip-flops (`ex_af`, `reg_sel_exx`, `ex_dehl_combined`) survive `reset()`, so a capture depends on what ran before. Add a cold reset that sets every dynamic node and latch to a defined state, as a power-on would.

## 3. Capture and analysis tools (from the agent proposals)

### 3.1 How the simulator decided a value (P1)

Two published results (IFF2 after NMI at EI, and X/Y after SCF/CCF) were decided by the simulator's own tie-break rules, not by the circuit, and were found by accident. These tools make every such case visible.

- **Resolution log.** `z80_resolution_events({from_hc, to_hc, nets?})`: every half-cycle where a net group was resolved by a rule rather than a single clear driver: a group touching both VCC and GND (the last supply reached wins), and undriven groups with different stored values merged (the member with the most gates wins). Each row: group members, the on transistors to each supply, the candidates, the value picked, and the rule. Script `resolutionLog(true)` to turn it on.
- **Driven or held.** A sample option that reports per net and half-cycle whether the value was driven or held as charge (for example `0`/`1` driven, `o`/`i` held). Today a floating value is reported only for nets flagged can-float, so `ubus3` never shows as undriven.
- **`z80_net_group({net})`**: the connected group at the current half-cycle, whether it is driven, the conducting devices, and the rule that set its value.
- **`z80_last_driver({net, hc})`**: the last half-cycle in which a channel on the net conducted, and which transistor it was.

### 3.2 Knockout (P1)

- **`z80_force_net({net, value: 0|1|"float", from_hc, to_hc})`**, cleared by reset, composing with `z80_sample_window(reset=false)`. It turns inferences into measurements, such as "if 1534 is held low in M3T1, does the result change". Script `forceNet(...)`.

### 3.3 Capture in one call (P2)

- **`z80_capture({mem, nets, buses?, halfcycles, pins?})`**: writes the program, resets, runs, and returns one string per net (`"0101zz..."`) plus hex per named byte group (`reg_a`, ALUBUS, VBUS, register pairs, with `Z` for floating members), the M/T label and the opcode fetched at each M1. This replaces the `z80_eval_js` preload plus `z80_sample_window` pattern, whose per-sample JSON arrays are large and hard to read.
- **Event and diff forms**: return only the half-cycles where an event net is high or a state net changes (`z80_trace_events`, `z80_sample_diff` in `How-Z80-Ticks/archive/audit_2026_09/reports/flags-decode.md`).
- **`z80_run_program`**: final registers, including the physical EX/EXX swap state and a memory range, for the result column of a capture.
- **`z80_insn_map({hc_from, hc_to})`**: split a capture into instructions and M-cycles, with prefixes merged.
- **`z80_changed_nets({hc})`**: every net of the 3,597 that changed between hc-1 and hc, to find latches that load in a given half-cycle without guessing candidates. Needs a whole-chip state snapshot per half-cycle, or a re-run to that point.
- **`z80_bus_readers({nets, hc_from, hc_to})`**: every pass transistor touching the nets whose gate is on in the window ("who samples this bus now").

### 3.4 Topology helpers (P2)

- **`z80_gate_classify({nets})`**: inverter, NOR, NAND, pass-XNOR, clocked refresh latch, superbuffer or dynamic node, with inputs and polarity. Recognising the two XNORs and the refresh loops in the flag logic took several `z80_net_drivers` rounds each. This is the C++ form of the classifier the wishlist lists as open.
- **`z80_equation` with `depth` and `stop_at_named`**: full expansions ran to tens of kilobytes.
- **`z80_pulldown_terms({nets})`**: product terms through series nodes.
- **`z80_equation` labels a push-pull stage as a clock gate when no clock is involved.** The clocked push/pull detection in `ClassLogic.cpp` (the block that builds `LogicOp::ClkGate`) matches a net without a pull-up that has two pull-downs and one device to VCC whose gate net has a pull-up and two pull-downs, and never checks that one of the two pull-downs is gated by `clk`. Net 110 `_last_t` (pull-downs gated by `last_t` and `int_reset`) prints as `CLKGATE`. Require `clk` on one pull-down.
- **`z80_px_rows({net})`**: the PLA rows behind a `px*` net and whether they are ORed or NORed.

## 4. Physical layout API (P1)

The layer bitmaps and `layermap.bin` are all loaded in the app, but a render is the only way to see them. The agents rebuilt geometry offline from PNGs, which is slow and imprecise (`iff2-fight_wl.py`, `iff2-fight_loads.py` in `How-Z80-Ticks/archive/audit_2026_09/scripts/`).

- **`z80_probe({x, y})`**: what is at a die pixel on each layer: net, transistor, via or buried contact. It is the MCP form of clicking on the die.
- **`z80_trans_geometry({ids})`**: box, gate area, W, L, W/L, orientation, source and drain edge lengths per net, and whether the gate is bent; computed from diffusion AND poly inside the box. Script `transGeom(id)`.
- **`z80_pullup_info({nets})`**: the depletion load of each pulled-up net with box, W, L and W/L. Pull-ups have no transistor record (`loadTransdefs` skips them), so today their geometry needs a pixel search.
- **`z80_layer_crop({x, y, w, h, layers, encoding: "rle"|"bitrows"})`**: the layer bitmaps as data, one bit per pixel per layer.
- **`z80_net_pixels({net, layer})`**: area, bounding box and connected components of a net on each layer, from `layermap.bin`.
- **`z80_net_route({net, from, to})`**: the path through a net's layers as `[{layer, length_px, width_px, squares}]`, to estimate wire resistance.
- **`z80_ratio_report({net, params})`**: at the current state, the conducting group, every on device to VCC and GND with W/L, series pass chains, and the DC-solved node voltages next to the switching threshold of each inverter that reads them. It answers the fight questions the tie-break rules decide today.
- **`z80_die_info`** should say that the ions layer holds only the trap implants.

## 5. Simulator model (P3, decision needed)

The switch-level model has no device strength, so a fight or a charge-sharing merge is decided by a fixed rule. With `z80_trans_geometry` and `z80_pullup_info` in place, an optional strength-aware mode could resolve a group by W/L ratio, and the resolution log (3.1) could report where the two modes disagree. That is a model change, and the fixed rules should stay the default.

## 6. Working with several agents (P2)

- **A simulator lease in the server.** Agents shared one simulator through the `sim_lock.ps1` / `sim_unlock.ps1` scripts (archived in `How-Z80-Ticks/archive/audit_2026_09/`). A `z80_lease({owner, ttl_s})` / `z80_release` pair in the MCP server, with state-changing tools refusing a caller without the lease, would replace them and survive a crashed agent through the timeout.
