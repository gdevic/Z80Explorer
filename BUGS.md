# BUGS.md — triaged defect list

Every item from the two December 2025 static-analysis passes, re-checked line by line against the Z80Explorer source at commit `b5b8779` (2026-09-21). The original line numbers were stale, so each item was relocated by its quoted snippet; the file and line references below are current.

No source was changed. This is a worklist, not a changelog.

| Status | Count | Meaning |
|---|---:|---|
| Present | 21 | Defect confirmed in the current source. |
| Fixed | 4 | Gone, attributed where a commit could be identified. |
| Invalid | 7 | The original claim was wrong about what the code does. |
| **Total** | **32** | |

**Read the Invalid section before acting on anything here.** Seven of the eight items the December 2025 static-analysis worklist ranked Critical or Medium do not survive review, and three of them (`m_flashTimeLeft`, `m_traceback`, `m_heartbeat`) name symbols that have never existed in this codebase at any commit. Treat the rest of that document's confidence accordingly.

## Present — thread safety

These four share one root cause: the simulation loop runs on a worker thread while the UI reads the same state, and nothing is protected. There is no `QMutex` anywhere in `ClassSimZ80` or `ClassNetlist`. Fixing them individually is probably wasted effort; the decision is whether to guard the shared state or to hand the UI its own snapshot.

**1. Simulation loop races the UI.** `src/ClassSimZ80.cpp:120` runs `QtConcurrent::run` over `halfCycle()`, which writes the netlist, the transistor array and the watch data at `src/ClassSimZ80.cpp:208`. Only `m_runcount`, `m_hcyclecnt` and `m_hcycletotal` are atomic (`src/ClassSimZ80.h:93`). Impact: data races, wrong values on screen during a run, possible crashes.

**2. `ClassWatch::append` writes non-atomic members from the sim thread.** `src/ClassWatch.cpp:54` sets `m_hcycle_last` and `:59` sets `m_hring_start`; both are plain `uint` at `src/ClassWatch.h:68` and are read by the UI through `gethstart()`. Impact: torn reads, inconsistent waveform. Note the history depth is now the runtime `m_historyDepth` rather than the old `MAX_WATCH_HISTORY` constant, which moved to `AppTypes.h` in `f027a39`; the race itself is unchanged.

**3. Static disassembler state in `formatBus`.** `src/ClassController.cpp:308` declares `static bool wasED` and `wasCB`, used at `:318` and written at `:320`. Two waveform docks decoding at once corrupt each other's prefix tracking. Impact: wrong disassembly text.

**4. File-scope mutable state in the logic walker.** `src/ClassLogic.cpp:19` holds `static QSet<net_t> visitedNets` and `visitedTrans`, cleared at `:202`. Two concurrent or re-entrant `getLogicTree()` calls interfere. Impact: wrong equations, possible non-termination. The container type was upgraded to `QSet` for speed, which did not touch this defect.

## Present — memory and lifetime

**5. `m_p3[0..2]` are never freed.** Allocated at `src/ClassVisual.cpp:87`; `ClassVisual` has no destructor and no `delete[]` exists. Roughly 600 MB. In practice an at-exit leak only, because the object lives for the whole process inside the global controller, so this is low urgency.

**6. Fragile ownership in `versionCheck`.** `src/MainWindow.cpp:250` creates a `unique_ptr<QNetworkAccessManager>` and `:266` releases it inside a short-circuit `&&` chain. If the `connect` fails the manager is destroyed while the pending reply still refers to it.

## Present — correctness

**8. `loadHex` never checks its conversion status.** `src/ClassTrickbox.cpp:314` declares `bool bStatus` and `:324` onward pass it to four `toUInt()` calls, none of which is ever tested. A malformed HEX file silently loads garbage into simulated RAM.

**9. No bounds check in `ClassNetlist::get`.** `src/ClassNetlist.h:47` indexes `m_netnames[n]` directly against a `MAX_NETS` array. Other accessors added later do guard (`src/ClassNetlist.h:63` and `:71` both test `n && n < MAX_NETS`); this one was not brought in line.

**10. Division by zero in the rate estimate.** `src/ClassSimZ80.cpp:73` divides by `m_elapsed.elapsed()` with no guard. On IEEE hardware this yields infinity or NaN rather than a trap, so the symptom is a nonsense frequency readout, not a crash.

**12. Unchecked table item in the waveform cursor.** `src/DockWaveform.cpp:258` takes `tv->item(row, 1)` and `:263` calls `setText` on it with no null test.

**13. `shrinkVias` walks without an index bound.** `src/ClassVisual.cpp:711` advances `pix` while the via bit is set, with no limit. Mitigated in practice by the one-pixel edge trim at `:694` that zeroes the border rows and columns, so a run terminates before the buffer end. The missing explicit bound is still worth closing.

**14. Underflow in the flood-fill seed list.** `src/ClassVisual.cpp:950` appends `uint16_t(pos.y - 1)` and `uint16_t(pos.x - 1)`, which wrap to 65535 at the edges, and the popped value is used unguarded at `:944`.

**15. Release builds fall through a disabled assert.** `src/ClassVisual.cpp:1304` has `Q_ASSERT(0)` followed by `return 0`. With asserts compiled out, the callers at `:1315` and `:1327` feed that zero straight back into a `while (true)` walk. Impact: hang during transistor path generation.

**16. `startDetached` error reporting is meaningless.** `src/ClassScript.cpp:287` uses the detached start, and the failure path at `:298` reports `process.errorString()`, which is not populated for that call.

**17. Watch history arithmetic ignores wraparound.** `src/ClassWatch.cpp:58` computes `m_hring_start` in plain 32-bit unsigned arithmetic. After about four billion half-cycles the ring indexing breaks. Theoretical at current simulation speeds.

**18. `chop(1)` on an empty bus removes the bracket.** `src/ClassNetlist.cpp:117` builds `"N: ["`, appends one entry per member, then unconditionally chops the trailing comma. An empty bus loses its `[` instead and writes malformed `netnames.js`.

**C5. `_bittest64` is handed offsets far beyond 63.** `src/ClassSimZ80_AVX2.cpp:78` and `:84` pass a `net_t` up to 3596 into `_bittest64` and `_bittestandset64`. MSVC lowers these to the memory-operand `BT`/`BTS` form, whose bit offset is architecturally allowed to exceed the operand size, so on MSVC x64 it does what was intended. It is still undefined by the intrinsic's contract and would break under another compiler; the AVX2 path is Windows-only, which is what keeps it safe today.

## Present — style and cleanliness

**11. Misleading comment in the transistor swap.** `src/ClassNetlist.cpp:256` reads `// ngnd=1, npwr=2, nclk=2` while the code asserts `clk` is net 3. The logic is right, the comment is not.

**H3. Redundant `QSettings` construction.** Multiple local instances per file: `src/DockLog.cpp` at `:20, :71, :83, :101, :116`; `src/DockWaveform.cpp` at `:25, :103`; `src/DockImageView.cpp` at `:12, :20`; `src/MainWindow.cpp` at `:68, :118`. Two of the six originally listed files, `DockCommand.cpp` and `DockMonitor.cpp`, no longer construct any.

**M1. Early `return` where `continue` was suggested.** Six loops: `src/DialogEditAnnotations.cpp:113` and `:133`; `src/DialogEditColors.cpp:154` and `:173`; `src/DialogEditWaveform.cpp:178` and `:198`. Judge this one carefully rather than applying the suggested fix. Because the row list is sorted toward the boundary first, "move the whole block or nothing" is defensible semantics for a contiguous selection; it is only wrong for a non-contiguous one, and a blind `continue` would corrupt the ordering.

**M2. Missing null checks, partly confirmed.** Real instances: `src/WidgetImageView.cpp:321` dereferences `getSegment(net)`, which `src/ClassVisual.h:67` documents as returning null when not found, and the same unchecked pattern appears at `src/ClassVisual.cpp:877` and `:891`; `src/DialogSchematic.cpp:49` and `:56` call `.first()` on a possibly empty selection guarded only by `Q_ASSERT`. Not confirmed: `WidgetImageOverlay.cpp` is now 125 lines and the cited location no longer exists; the cited `ClassVisual` site is `getNetsAt()`, which has had a bounds guard since 2020 at `src/ClassVisual.cpp:379`; the other `WidgetImageView` sites are guarded.

## Fixed

**7. Comment stripping now looks for `//`.** `src/ClassNetlist.cpp:150` reads `line.indexOf("//")`, and the text after it is captured as a per-net tip. Fixed by `14dab36`, "Handle tips as comments in netnames.js".

**C4. Floating-node resolution is implemented.** The `#if 0` block is gone. Both variants actively resolve by connection count: the performance path at `src/ClassSimZ80.cpp:311` and the fallback at `:349`. Fixed by `a214950`, "Correction to the sim engine".

**H1. Logic-walker duplicate detection is O(1).** `src/ClassLogic.cpp:19` uses `QSet` with `contains()` and `insert()`. The `QVector::contains()` form is gone. The original list already marked this fixed; verified.

**H2. `netsDriving` and `netsDriven` are O(1) per insert.** `src/ClassNetlist.cpp:467` and `:487` build a `QSet<net_t>` then convert to a sorted vector. No `nets.contains()` remains. Already marked fixed; verified.

## Invalid

**C1. No use-after-free of the log window.** `src/MainWindow.cpp:42` explicitly reparents the dock, and `addDockWidget` transfers ownership to the main window. `delete mainWindow` at `src/main.cpp:77` already runs before `delete wndInit` at `:83`, so the log window dies with its real parent first.

**C2. `getFirst` does not skip the first element.** `src/ClassWatch.h:37` returns `m_watchlist.data()`, which is the address of element zero. Setting `it = 1` correctly positions `getNext` at the second element. Nothing is skipped.

**C3. The waveform iterator is likewise correct.** `src/WidgetWaveform.cpp:82` walks every item including index zero, using the same idiom.

**C6. No null dereference of the watch pointer.** `ClassWatch::at()` guards its argument at `src/ClassWatch.cpp:73`, and the bus overload at `:94`. Both guards date from 2020, so the claim was wrong when it was written. The genuine defect on those lines is item 12 above, the unchecked table item.

**C7. `m_trickWriteEven = ab & 1` is correct.** The intent, stated in the comment above it, is to validate only on the second memory access of each two-byte word. Trickbox control words are even-aligned, so the second byte lands on an odd address and the flag is meant to be true there. Applying the proposed `!(ab & 1)` would invert the behaviour and break the trickbox. Only the member name is a misnomer.

**M3. `m_flashTimeLeft` and `m_traceback` do not exist.** Neither symbol appears in the source, and `git log -S` finds no commit that ever introduced either. `src/WidgetToolbar.cpp` is 54 lines with no timing arithmetic, and `ClassTrickbox` has no traceback member.

**M4. `m_heartbeat` does not exist either.** The real code at `src/WidgetToolbar.cpp:31` toggles off the current stylesheet, `ui->btRun->setStyleSheet(ui->btRun->styleSheet().isEmpty() ? "background-color: lightgreen" : "")`, which is a correct self-toggling flash, cleared on stop at `:42`. Unchanged since 2020.

## Suggested order

1. Decide the thread-safety strategy once and apply it to items 1 through 4 together. Either guard the shared state or give the UI a snapshot; patching them one at a time will not converge.
2. Item 15, the release-build assert fallthrough, is the only remaining item that can hang the application.
3. Items 8, 9, 12, 13 and 14 are cheap, local bounds and null checks.
4. Items 5, 6, 10, 11, 16, 17, 18 and the style group are low priority.
5. Item C5 needs attention only if the AVX2 path is ever built with a compiler other than MSVC.
