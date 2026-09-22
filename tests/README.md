# Tests

## `mcp_integration.py`

Integration and conformance suite for the built-in MCP server. It drives a running Z80 Explorer over HTTP, so it is a local check rather than something CI can run: the app needs its resources loaded and the only workflow in `.github/workflows/` builds without a display.

The MCP tools couple tightly to the global `ClassController`, so isolating them for unit tests would need extensive stubbing. Driving the real server is both cheaper and a truer check.

### Running it

1. Build and launch `Z80Explorer.exe`. The MCP server is enabled by default; if it was switched off, re-enable it under Edit then Settings.
2. Wait until the endpoint answers — `curl http://localhost:8765/mcp/healthz`, or watch for `MCP server listening` in the Log dock. Startup loads six 4700x5000 images and builds the layer map, so it is not instant.
3. `python tests/mcp_integration.py`

| Flag | Effect |
|---|---|
| `--port N` | server on a non-default port |
| `--host H` | server on a non-default address |
| `--quick` | skip the three concurrency cases |
| `--only SUBSTR` | run only tests whose name contains SUBSTR |

Standard library only, deliberately: no `pip install` stands between a checkout and a verified server.

### What it covers

**Conformance** — the MCP 2026-07-28 wire contract. Per-request `_meta` enforcement, protocol-version negotiation, the standard request headers and their error code, the `CacheableResult` fields, `resultType`, Origin and Content-Type policy, batching rejection, notification handling, and the transport's method rules.

**Tools** — every registered tool, the batched list forms and their caps, waveform capture, the die renderer across all twenty layers, and the concurrency cases that exercise the nested event loop `m_toolBusy` guards.

**Regressions** — each fixed bug has a case that fails if it comes back. Those carry a docstring saying what the bug was, so a future failure is legible without archaeology.

### Assertions that encode chip facts

A few cases assert things about the Z80 netlist rather than about the code, and will fail if the netlist data changes:

- at least 99 nets match `^pla[0-9]+$`
- net 210 (`ixy_d_phase`) has a fanout of 6 and is gated by `ctl_m6`
- `ixy_d_phase` first asserts at `M2T2` when running `ini`
- an equation tree over net 210 has `m2` and `t2` among its leaves

Treat a failure there as a question about the data, not a bug in the server.
