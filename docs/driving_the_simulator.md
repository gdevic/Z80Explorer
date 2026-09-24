# Driving the simulator from outside

The app can be driven three ways: the MCP server, the raw TCP command socket, and JavaScript run inside the app. The User's Guide (`docs/Z80Explorer.*`) covers the interactive application; this page covers what a script or an agent needs to know.

## MCP over HTTP, port 8765

The main path. It binds to 127.0.0.1 only and is enabled by default; the checkbox under Edit then Settings is the off switch. The repository has a project-scoped `.mcp.json` pointing at `http://localhost:8765/mcp`, so no `claude mcp add` is needed. The server speaks MCP revision `2026-07-28` and only that one: no `initialize` handshake, every request carrying its protocol version and client capabilities in `_meta`. Claude Code negotiates it natively.

**Start the app before starting the session.** A session that starts while the app is closed gets `ConnectionRefused`, and the client does not retry a server that failed at startup, so the tools stay dead for that whole session even after the app comes up. Reconnect the server, or start the app first.

Tool behaviour worth knowing before relying on it:

- `z80_net_info`, `z80_net_drivers`, `z80_fanout` and `z80_equation_tree` answer topology questions. The query tools take a list (`nets`, or `ids`) as well as a single target, so characterising a group of nets is one call rather than one per net.
- `z80_sample_window` is the way to trace; it returns `hc` and `mt` arrays alongside the samples. It returns a constant for bus names such as `INSTR`, so capture the bit nets.
- `z80_equation` prints a series pull-down chain as `NAND(a,b)` and a single pull-down as `INV(x)` inside a `NOR`; read the drivers before treating its output as ordinary Boolean.
- `z80_die_info` describes the image, the coordinate system and the layer names; read it before making a spatial claim. `z80_view_render` draws any region with chosen layers and highlighted nets (`draw_latches` overlays the `latches.ini` registry).
- `z80_eval_js` runs JavaScript in the app and is the escape hatch for anything without a tool.
- `z80_rename_net` renames, and `z80_save` persists names, comments and the watchlist without quitting the app; it is safe mid-run. `z80_save {items:["netnames"]}` writes only the names, buses and comments; add `"watchlist"` after a rename, since a rename also renames the net's watchlist entry and every newly named net is added to the watchlist.
- `z80_reset` and the script `reset()` clear a pending `z80_pin_set_at_pc` schedule, so arm the pin after the reset. They do not clear F or the register-select flip-flops (`ex_af`, `reg_sel_exx`, `ex_dehl_combined`), so a capture that reads `reg_a` must also account for `reg_aa`.

When several clients share one simulator, serialise every stateful call (memory writes, reset, run, sample, pin, an eval that runs) behind one lock; topology queries are stateless and need no lock.

`tests/mcp_integration.py` is the conformance and regression suite, stdlib only; three of its cases run only with `--destructive`, which lets `z80_save` rewrite the live user data files. Run it against a live app after touching anything in the MCP layer. `TODO.md` holds the proposed tools.

## Raw TCP command socket, port 12345

The older path. A client sends JavaScript one-liners such as `run`, `readBits`, `writeMem`, `mon.setAtPC` and parses the CSV replies. `resource/scripts/sock.py` is an example client.

## In-app JavaScript

Load a script from the Command dock with `load("path")`, or drop it on the app. Script cwd is the app's `resource/`, so paths inside JS need the subfolder prefix, for example `loadHex("tests/hello_world.hex")`.

- `run(n)` starts the simulation and returns at once; a script that reads state right after it sees only the half-cycles already done. Step with `run(1)` in a loop when the reads must line up.
- `setNetName` cannot overwrite an existing name. To rename a named net, use `renameNet` (the `z80_rename_net` tool). Both report a refusal only in the log.
- Net comments are tips, and no script or MCP call sets one. Dropping a JSON file with a `tips` key onto a die image view loads it, and `z80_save {items:["netnames"]}` then writes the comments into `netnames.js` (`name: id, // comment`). The drop also makes that file the tips save target until the app restarts, so copy it over `resource/user/tips.json` afterwards.
- `saveNetnames()` writes `resource/chip/netnames.js` and `resource/user/tips.json` together; it is the same as `save("netnames")` or `z80_save {items:["netnames"]}`. The app also writes every user data file from `MainWindow::closeEvent` on a clean exit.
