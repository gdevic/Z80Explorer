# Z80 Explorer MCP tests

End-to-end integration tests for the built-in MCP (Model Context Protocol)
server. Tests drive a running Z80 Explorer instance over HTTP, exercising
every registered tool plus several concurrency / threading stress scenarios.

## Why integration tests instead of Qt Test units?

MCP tool handlers tightly couple to the global `ClassController` and its
subsystems (sim, netlist, trickbox, chip visual, annotations, watch, ...).
Isolating them for pure C++ unit tests would require either linking the
whole app into the test binary or building extensive stubs for every
subsystem. The Python integration suite tests the real behaviour with
no stubbing: correctness, error handling, schema conformance, PNG
decoding, and — crucially — threading behaviour under concurrent load.

## Running

Prerequisites:
- Z80 Explorer built with `MCP_SERVER 1` in `src/AppTypes.h` (the default).
- `pip install requests` (Python 3.9+).

Steps:

1. Launch `Z80Explorer.exe`.
2. Confirm the log shows `MCP server listening on http://localhost:8765/mcp`.
3. From the repo root run:
   ```
   python tests/mcp_integration.py
   ```

Options:
- `--port N`       — connect to a non-default port.
- `--quick`        — skip concurrency / long-running tests.

## What's covered

### Protocol-level

- `GET /mcp/healthz` liveness probe
- `initialize` negotiation
- `tools/list` — confirms all 26 default tools present
- `ping` round-trip
- JSON-RPC error path for unknown methods

### Tool coverage (all 26 typed tools)

- Execution: `load_hex`, `reset`, `run`, `stop`, `now`
- State reads: `net_read`, `bus_read`, `register_read`, `trans_read`, `waveform_window`
- Metadata: `net_find`, `net_info`, `trans_info`, `equation`
- Memory/IO: `mem_read/write`, `io_read/write`
- Pin control: `pin_set`, `pin_set_at`, `pin_set_at_pc`
- Breakpoints: `break_add`, `break_clear`
- Image view: `view_set`, `view_grab`
- Escape hatch: `eval_js`

Each tool is invoked with representative arguments; return shapes and
error paths are asserted.

### Threading / concurrency

These matter because the Z80 simulator runs on a `QtConcurrent::run()`
worker thread while MCP handlers arrive on the HTTP server thread.
Handlers marshal to the main thread via `ClassMcpThreading::callOnMain()`
with `Qt::BlockingQueuedConnection`. The suite stress-tests that boundary:

- **concurrent net reads** — 200 parallel `z80_net_read` calls from
  8 worker threads. Verifies no crashes, no invalid values, and no
  `id` correlation mix-ups at the HTTP layer.
- **concurrent mixed calls** — 100 interleaved reads (`now`, `register_read`,
  `bus_read`, `net_read`, `mem_read`) across 4 threads.
- **stop while running** — thread A issues `z80_run(10M hc)`, thread B
  calls `z80_stop`; A's call must return with `stopped_by=stop` within
  timeout.
- **render during run** — kicks off a 50_000 half-cycle run, then
  renders three regions in parallel with the sim worker. Verifies the
  Qt graphics pipeline serialises correctly on the main thread.

## Expected output

A passing run looks like:

```
Connecting to http://localhost:8765/mcp
healthz                                           OK (3 ms)
initialize                                        OK (2 ms)
tools/list                                        OK (1 ms)
ping                                              OK (1 ms)
...
concurrent net reads                              OK (421 ms)
concurrent mixed calls                            OK (187 ms)
stop while running                                OK (412 ms)
render during run                                 OK (3510 ms)

Passed: 33, Failed: 0
```

## What's NOT tested here

- Legacy `ClassServer` TCP socket (port 12345, `SOCKET_SERVER=0` by default).
  Old Python scripts in `resource/*_client.py` and `test_*_socket.py` remain
  the verification for that path and are unaffected by this change.
- Interactive UI behaviour — that's manual testing.
- Rendering pixel accuracy — we assert the result is a valid PNG of the
  requested dimensions but don't diff pixels (out of scope).
