#!/usr/bin/env python3
"""
MCP integration test suite for Z80 Explorer.

Drives the running Z80 Explorer MCP server over HTTP, exercising every
registered tool plus several concurrency and threading-stress scenarios.
This is the primary correctness check for the MCP subsystem since the
tools tightly couple to the global ClassController — isolating them for
pure unit tests would require extensive stubbing.

Usage:
    1. Build and launch Z80Explorer.exe (with MCP_SERVER=1 in AppTypes.h).
    2. Wait until the startup message "MCP server listening on
       http://localhost:8765/mcp" appears in the log dock.
    3. Run this script:  python tests/mcp_integration.py
    4. Optional: pass --port N if the server is on a non-default port,
       --quick to skip the slow concurrency tests.

Requires: Python 3.9+, the `requests` package.
"""

import argparse
import concurrent.futures
import json
import os
import random
import sys
import threading
import time
from dataclasses import dataclass, field
from typing import Any

import requests


# --------------------------------------------------------------------------
# JSON-RPC client
# --------------------------------------------------------------------------

@dataclass
class McpClient:
    url: str
    session: requests.Session = field(default_factory=requests.Session)
    next_id: int = 1
    lock: threading.Lock = field(default_factory=threading.Lock)

    def call(self, method: str, params: dict | None = None, timeout: float = 60) -> dict:
        with self.lock:
            rid = self.next_id
            self.next_id += 1
        body = {"jsonrpc": "2.0", "id": rid, "method": method}
        if params is not None:
            body["params"] = params
        r = self.session.post(self.url, json=body, timeout=timeout)
        r.raise_for_status()
        return r.json()

    def tool(self, name: str, arguments: dict | None = None, timeout: float = 60) -> dict:
        resp = self.call("tools/call", {"name": name, "arguments": arguments or {}}, timeout=timeout)
        if "error" in resp:
            raise RuntimeError(f"RPC error calling {name}: {resp['error']}")
        return resp["result"]

    def tool_text(self, name: str, arguments: dict | None = None, timeout: float = 60) -> dict:
        """Helper: call a tool expecting its first content block to be JSON-as-text."""
        result = self.tool(name, arguments, timeout=timeout)
        if result.get("isError"):
            text = result["content"][0].get("text", "<no text>")
            raise RuntimeError(f"Tool {name} reported error: {text}")
        text = result["content"][0]["text"]
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return {"_raw": text}


# --------------------------------------------------------------------------
# Test runner
# --------------------------------------------------------------------------

class Suite:
    def __init__(self, client: McpClient):
        self.c = client
        self.passed = 0
        self.failed: list[str] = []

    def run(self, name: str, fn):
        sys.stdout.write(f"{name:<50}")
        sys.stdout.flush()
        try:
            t0 = time.time()
            fn()
            elapsed = time.time() - t0
            print(f"OK ({elapsed*1000:.0f} ms)")
            self.passed += 1
        except Exception as e:
            print(f"FAIL: {e}")
            self.failed.append(f"{name}: {e}")

    def summary(self) -> int:
        print()
        print(f"Passed: {self.passed}, Failed: {len(self.failed)}")
        for f in self.failed:
            print(f"  - {f}")
        return 0 if not self.failed else 1


# --------------------------------------------------------------------------
# Individual tests
# --------------------------------------------------------------------------

def test_healthz(client: McpClient):
    r = requests.get(client.url.rstrip("/mcp") + "/mcp/healthz", timeout=5)
    r.raise_for_status()
    data = r.json()
    assert data["status"] == "ok", data
    assert data["tools"] > 0, data


def test_initialize(client: McpClient):
    resp = client.call("initialize")
    assert "result" in resp, resp
    info = resp["result"]
    assert "protocolVersion" in info
    assert info["serverInfo"]["name"] == "z80explorer"


def test_tools_list(client: McpClient):
    resp = client.call("tools/list")
    tools = resp["result"]["tools"]
    assert len(tools) >= 20, f"expected >=20 tools, got {len(tools)}"
    names = {t["name"] for t in tools}
    required = {
        "z80_reset", "z80_run", "z80_stop", "z80_now",
        "z80_net_read", "z80_bus_read", "z80_register_read",
        "z80_mem_read", "z80_mem_write",
        "z80_pin_set",
        "z80_net_find", "z80_net_info", "z80_trans_info", "z80_equation",
        "z80_region_of", "z80_nets_near", "z80_trans_near", "z80_bounding_box",
        "z80_eval_js",
    }
    missing = required - names
    assert not missing, f"missing tools: {missing}"


def test_ping(client: McpClient):
    resp = client.call("ping")
    assert "result" in resp


def test_unknown_method(client: McpClient):
    resp = client.call("bogus/method")
    assert "error" in resp, resp
    assert resp["error"]["code"] == -32601


def test_reset(client: McpClient):
    data = client.tool_text("z80_reset")
    assert data["ok"] is True, data


def test_now(client: McpClient):
    data = client.tool_text("z80_now")
    for key in ("hc", "pc", "mt", "running"):
        assert key in data, data


def test_net_read(client: McpClient):
    data = client.tool_text("z80_net_read", {"nets": ["clk", "vss", "vcc", 1, 2]})
    assert "results" in data and len(data["results"]) == 5
    for r in data["results"]:
        assert "value" in r
        assert r["value"] in (0, 1, 2, 3)


def test_bus_read(client: McpClient):
    data = client.tool_text("z80_bus_read")
    for key in ("M1", "MREQ", "IORQ", "RD", "WR", "RFSH", "HALT"):
        assert key in data


def test_register_read(client: McpClient):
    data = client.tool_text("z80_register_read")
    for key in ("PC", "AF", "BC", "DE", "HL", "SP", "IX", "IY", "IR", "WZ", "instr"):
        assert key in data


def test_trans_read(client: McpClient):
    data = client.tool_text("z80_trans_read", {"ids": [1, 2, 3]})
    assert len(data["results"]) == 3


def test_net_find(client: McpClient):
    data = client.tool_text("z80_net_find", {"pattern": "^clk$"})
    assert data["total"] >= 1
    data2 = client.tool_text("z80_net_find", {"pattern": "^ab[0-9]+$"})
    assert data2["total"] == 16


def test_net_info(client: McpClient):
    data = client.tool_text("z80_net_info", {"net": "clk"})
    assert data["id"] != 0, data


def test_trans_info(client: McpClient):
    data = client.tool_text("z80_trans_info", {"id": 1})
    assert "on" in data


def test_equation(client: McpClient):
    data = client.tool_text("z80_equation", {"net": "clk"})
    assert "expr" in data


def test_mem_roundtrip(client: McpClient):
    client.tool_text("z80_mem_write", {"addr": 0x100, "bytes": [0x01, 0x23, 0x45, 0x67]})
    r = client.tool_text("z80_mem_read", {"addr": 0x100, "len": 4})
    assert r["bytes"] == [0x01, 0x23, 0x45, 0x67]


def test_io_roundtrip(client: McpClient):
    client.tool_text("z80_io_write", {"addr": 0x42, "byte": 0xAB})
    r = client.tool_text("z80_io_read", {"addr": 0x42})
    assert r["byte"] == 0xAB


def test_pin_set(client: McpClient):
    client.tool_text("z80_pin_set", {"pin": "INT", "value": 1})
    client.tool_text("z80_pin_set", {"pin": "INT", "value": 0})


def test_run_short(client: McpClient):
    client.tool_text("z80_reset")
    data = client.tool_text("z80_run", {"halfcycles": 20, "timeout_ms": 5000})
    assert data["ok"] is True, data
    assert data["hc_final"] >= 20


def test_run_until_cycle(client: McpClient):
    client.tool_text("z80_reset")
    data = client.tool_text("z80_run", {"halfcycles": 0, "until": {"cycle": 50}, "timeout_ms": 5000})
    assert data["stopped_by"] in ("cycle", "count")
    assert data["hc_final"] >= 50 - 2


def test_break_add_clear(client: McpClient):
    r = client.tool_text("z80_break_add", {"cycle": 100})
    assert "id" in r
    bid = r["id"]
    r2 = client.tool_text("z80_break_clear", {"id": bid})
    assert bid in r2["cleared"]


def test_region_of(client: McpClient):
    data = client.tool_text("z80_region_of", {"net": "clk"})
    assert data["valid"] in (True, False)  # just exercise it


def test_nets_near(client: McpClient):
    data = client.tool_text("z80_nets_near", {"x": 2350, "y": 2500, "radius": 200})
    assert "hits" in data


def test_trans_near(client: McpClient):
    data = client.tool_text("z80_trans_near", {"x": 2350, "y": 2500, "radius": 200})
    assert "hits" in data


def test_bounding_box(client: McpClient):
    data = client.tool_text("z80_bounding_box", {"ids": [1, 2, 3], "type": "trans"})
    assert "bbox" in data


def test_eval_js(client: McpClient):
    data = client.tool_text("z80_eval_js", {"snippet": "print('hello from js'); 1+1"})
    assert data["ok"] is True
    assert "hello from js" in data["stdout"]


def test_fanout(client: McpClient):
    # clk is gate of many transistors — should return a non-trivial list
    data = client.tool_text("z80_fanout", {"net": "clk"})
    assert data["id"] != 0
    assert data["gate_count"] > 50, f"clk should gate dozens of transistors, got {data['gate_count']}"
    # Each entry should have the expected keys
    for e in data["gates"][:3]:
        assert "id" in e and "c1" in e and "c2" in e and "on" in e


def test_fanout_unknown(client: McpClient):
    resp = client.call("tools/call", {"name": "z80_fanout", "arguments": {"net": "bogus_xyzzy"}})
    r = resp.get("result", {})
    assert r.get("isError") is True


def test_watchlist_add(client: McpClient):
    data = client.tool_text("z80_watchlist_add", {"nets": ["reg_f0", "reg_f7"]})
    assert data["total"] >= 2
    # Adding the same nets again should go to 'skipped'
    data2 = client.tool_text("z80_watchlist_add", {"nets": ["reg_f0"]})
    assert "reg_f0" in data2["skipped"]


def test_sample_window(client: McpClient):
    data = client.tool_text("z80_sample_window", {
        "nets": ["clk", "reg_f0", "reg_f7"],
        "halfcycles": 20,
        "reset": True,
    }, timeout=30)
    assert "samples" in data
    for name in ("clk", "reg_f0", "reg_f7"):
        assert name in data["samples"], f"missing {name} in samples"
        # clk must have toggled across 20 halfcycles
    clk_samples = data["samples"]["clk"]
    assert len(set(clk_samples)) >= 2, f"clk should toggle, got {clk_samples[:5]}..."


def test_invalid_args(client: McpClient):
    # Missing required arg should produce an isError tool response, not RPC error
    resp = client.call("tools/call", {"name": "z80_load_hex", "arguments": {}})
    r = resp.get("result", {})
    assert r.get("isError") is True, resp


# --------------------------------------------------------------------------
# Concurrency / threading tests
# --------------------------------------------------------------------------

def test_concurrent_net_reads(client: McpClient, iterations: int = 200):
    """Fire many concurrent net_read calls. Verifies no crash, no invalid values."""
    def one(i):
        return client.tool_text("z80_net_read", {"nets": ["clk", "vss", "m1", "pc"]})

    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
        results = list(ex.map(one, range(iterations)))
    for r in results:
        for e in r["results"]:
            assert e["value"] in (0, 1, 2, 3)


def test_concurrent_mixed(client: McpClient, iterations: int = 100):
    """Mix of reads, register queries, small memory pokes — no serialization errors."""
    calls = [
        ("z80_now", {}),
        ("z80_register_read", {}),
        ("z80_bus_read", {}),
        ("z80_net_read", {"nets": ["clk"]}),
        ("z80_mem_read", {"addr": 0, "len": 16}),
    ]
    def one(i):
        name, args = calls[i % len(calls)]
        return client.tool_text(name, args)

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
        results = list(ex.map(one, range(iterations)))
    assert len(results) == iterations


def test_stop_while_running(client: McpClient):
    """Start a long run from thread A, stop from thread B, A returns with stopped_by=stop."""
    client.tool_text("z80_reset")

    stop_event = threading.Event()
    result_holder: list[dict] = []

    def runner():
        try:
            r = client.tool_text("z80_run", {"halfcycles": 10_000_000, "timeout_ms": 15000})
            result_holder.append(r)
        finally:
            stop_event.set()

    t = threading.Thread(target=runner)
    t.start()
    time.sleep(0.4)  # let the sim actually start running
    client.tool_text("z80_stop")
    t.join(timeout=20)
    assert not t.is_alive(), "runner did not stop within timeout"
    assert result_holder, "no result from runner"
    r = result_holder[0]
    assert r["stopped_by"] in ("stop", "count"), r  # allow "count" if the sim finished first


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--quick", action="store_true", help="skip concurrency / long-running tests")
    args = ap.parse_args()

    url = f"http://localhost:{args.port}/mcp"
    print(f"Connecting to {url}")
    client = McpClient(url)
    # Probe once so connect errors surface cleanly
    try:
        test_healthz(client)
    except Exception as e:
        print(f"Cannot reach Z80 Explorer MCP server: {e}")
        print("Is Z80Explorer.exe running with MCP_SERVER=1?")
        return 2

    suite = Suite(client)
    suite.run("healthz",               lambda: test_healthz(client))
    suite.run("initialize",            lambda: test_initialize(client))
    suite.run("tools/list",            lambda: test_tools_list(client))
    suite.run("ping",                  lambda: test_ping(client))
    suite.run("unknown method",        lambda: test_unknown_method(client))
    suite.run("z80_reset",             lambda: test_reset(client))
    suite.run("z80_now",               lambda: test_now(client))
    suite.run("z80_net_read",          lambda: test_net_read(client))
    suite.run("z80_bus_read",          lambda: test_bus_read(client))
    suite.run("z80_register_read",     lambda: test_register_read(client))
    suite.run("z80_trans_read",        lambda: test_trans_read(client))
    suite.run("z80_net_find",          lambda: test_net_find(client))
    suite.run("z80_net_info",          lambda: test_net_info(client))
    suite.run("z80_trans_info",        lambda: test_trans_info(client))
    suite.run("z80_equation",          lambda: test_equation(client))
    suite.run("z80_mem_roundtrip",     lambda: test_mem_roundtrip(client))
    suite.run("z80_io_roundtrip",      lambda: test_io_roundtrip(client))
    suite.run("z80_pin_set",           lambda: test_pin_set(client))
    suite.run("z80_run (short)",       lambda: test_run_short(client))
    suite.run("z80_run (until cycle)", lambda: test_run_until_cycle(client))
    suite.run("z80_break add+clear",   lambda: test_break_add_clear(client))
    suite.run("z80_region_of",         lambda: test_region_of(client))
    suite.run("z80_nets_near",         lambda: test_nets_near(client))
    suite.run("z80_trans_near",        lambda: test_trans_near(client))
    suite.run("z80_bounding_box",      lambda: test_bounding_box(client))
    suite.run("z80_eval_js",           lambda: test_eval_js(client))
    suite.run("z80_fanout",            lambda: test_fanout(client))
    suite.run("z80_fanout (unknown)",  lambda: test_fanout_unknown(client))
    suite.run("z80_watchlist_add",     lambda: test_watchlist_add(client))
    suite.run("z80_sample_window",     lambda: test_sample_window(client))
    suite.run("invalid args",          lambda: test_invalid_args(client))

    if not args.quick:
        print()
        print("Concurrency / threading stress tests (may take 10-30s):")
        suite.run("concurrent net reads",   lambda: test_concurrent_net_reads(client, 200))
        suite.run("concurrent mixed calls", lambda: test_concurrent_mixed(client, 100))
        suite.run("stop while running",     lambda: test_stop_while_running(client))

    return suite.summary()


if __name__ == "__main__":
    sys.exit(main())
