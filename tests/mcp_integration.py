#!/usr/bin/env python3
"""
MCP integration and conformance test suite for Z80 Explorer.

Drives the running Z80 Explorer MCP server over HTTP. The tools couple tightly to the global
ClassController, so isolating them for unit tests would need extensive stubbing; this suite is the
primary correctness check for the MCP subsystem instead.

Two halves:

  CONFORMANCE  the MCP 2026-07-28 wire contract - per-request _meta, the standard request headers,
               error codes, resultType, the CacheableResult fields, and the transport rules.
  TOOLS        every registered tool, plus regression cases for bugs that have been fixed.

Usage:
    1. Build and launch Z80Explorer.exe. The MCP server is enabled by default; if it was turned off
       in Edit > Settings, turn it back on.
    2. Wait for "MCP server listening on http://localhost:8765/mcp" in the log dock, or just poll
       http://localhost:8765/mcp/healthz.
    3. python tests/mcp_integration.py
       --port N       server on a non-default port
       --quick        skip the concurrency stress cases
       --destructive  also run the z80_save cases that write files
       --only SUBSTR  run only tests whose name contains SUBSTR

The suite is non-destructive by default. --destructive lets z80_save rewrite the live user data
files (resource/user/*.json and resource/chip/netnames.js), which are tracked in git: it persists
whatever the running app currently holds, so do not use it on an app whose files failed to load,
and expect a dirty working tree afterwards.

Requires Python 3.9+. Standard library only, deliberately: this must run with no pip install.
"""

import argparse
import base64
import concurrent.futures
import json
import sys
import threading
import time
import urllib.error
import urllib.request

PROTOCOL_VERSION = "2026-07-28"

# Reserved _meta keys this revision defines for the per-request protocol fields
META_VERSION = "io.modelcontextprotocol/protocolVersion"
META_CLIENT_CAPS = "io.modelcontextprotocol/clientCapabilities"
META_CLIENT_INFO = "io.modelcontextprotocol/clientInfo"
META_SERVER_INFO = "io.modelcontextprotocol/serverInfo"

# JSON-RPC and MCP error codes. The MCP specification reserves -32020..-32099 for itself.
PARSE_ERROR = -32700
INVALID_REQUEST = -32600
METHOD_NOT_FOUND = -32601
INVALID_PARAMS = -32602
HEADER_MISMATCH = -32020
MISSING_CLIENT_CAPABILITY = -32021
UNSUPPORTED_PROTOCOL_VERSION = -32022

# Results of these methods must carry ttlMs and cacheScope (the CacheableResult interface)
CACHEABLE_METHODS = ("tools/list", "prompts/list", "resources/list", "resources/templates/list")

OMIT = object()  # sentinel: "leave this out entirely", distinct from "set it to None"


class McpClient:
    """Minimal stateless-MCP client. Sends a conforming request unless told otherwise."""

    def __init__(self, url, version=PROTOCOL_VERSION):
        self.url = url
        self.version = version
        self._id = 0
        self._lock = threading.Lock()

    def _next_id(self):
        with self._lock:
            self._id += 1
            return self._id

    def meta(self):
        return {
            META_VERSION: self.version,
            META_CLIENT_INFO: {"name": "z80explorer-tests", "version": "1.0"},
            META_CLIENT_CAPS: {},
        }

    def post(self, method, params=None, meta=OMIT, headers=None, body=OMIT, rid=OMIT, timeout=60):
        """Returns (http_status, content_type, parsed_body_or_raw_text).

        meta=OMIT uses the conforming _meta; pass None to send no _meta at all, or a dict to
        override it. headers entries set to None delete that header. body overrides the whole body.
        rid=OMIT allocates an id; rid=None omits the id field entirely, making it a notification.
        """
        if body is OMIT:
            p = dict(params or {})
            if meta is OMIT:
                p["_meta"] = self.meta()
            elif meta is not None:
                p["_meta"] = meta
            payload = {"jsonrpc": "2.0", "method": method, "params": p}
            if rid is OMIT:
                payload["id"] = self._next_id()
            elif rid is not None:
                payload["id"] = rid
            data = json.dumps(payload).encode()
        else:
            data = body if isinstance(body, bytes) else json.dumps(body).encode()

        h = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            "MCP-Protocol-Version": self.version,
            "Mcp-Method": method,
        }
        if method == "tools/call" and params and "name" in params:
            h["Mcp-Name"] = params["name"]
        if method in ("resources/read",) and params and "uri" in params:
            h["Mcp-Name"] = params["uri"]
        for k, v in (headers or {}).items():
            if v is None:
                h.pop(k, None)
            else:
                h[k] = v

        req = urllib.request.Request(self.url, data=data, method="POST", headers=h)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as r:
                return r.status, r.headers.get("Content-Type"), _parse(r.read())
        except urllib.error.HTTPError as e:
            return e.code, e.headers.get("Content-Type"), _parse(e.read())

    def bare(self, method, path=None, timeout=15, headers=None):
        """A non-POST request to the endpoint, for the transport rules."""
        req = urllib.request.Request(path or self.url, method=method, headers=headers or {})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as r:
                return r.status, _parse(r.read())
        except urllib.error.HTTPError as e:
            return e.code, _parse(e.read())

    # ---- convenience wrappers that assert success -------------------------------------------

    def call(self, method, params=None, timeout=60):
        status, _ctype, body = self.post(method, params, timeout=timeout)
        assert status == 200, f"{method}: HTTP {status}, body {body}"
        assert "error" not in body, f"{method}: {body['error']}"
        return body["result"]

    def tool(self, name, arguments=None, timeout=60):
        return self.call("tools/call", {"name": name, "arguments": arguments or {}}, timeout=timeout)

    def tool_json(self, name, arguments=None, timeout=60):
        """A tool's payload: structuredContent when present, else the first text block parsed."""
        r = self.tool(name, arguments, timeout=timeout)
        if r.get("isError"):
            raise AssertionError(f"{name} reported error: {_first_text(r)}")
        if r.get("structuredContent") is not None:
            return r["structuredContent"]
        return _parse(_first_text(r).encode())

    def tool_raw(self, name, arguments=None, timeout=60):
        """A tool call that is allowed to fail; returns the JSON-RPC body."""
        _status, _ctype, body = self.post("tools/call", {"name": name, "arguments": arguments or {}},
                                          timeout=timeout)
        return body


def _parse(raw):
    if not raw:
        return None
    try:
        return json.loads(raw)
    except (json.JSONDecodeError, UnicodeDecodeError):
        return raw[:400].decode("utf8", "replace") if isinstance(raw, bytes) else raw


def _first_text(result):
    for c in result.get("content", []):
        if c.get("type") == "text":
            return c.get("text", "")
    return ""


def _err(body):
    assert isinstance(body, dict) and "error" in body, f"expected a JSON-RPC error, got {body}"
    return body["error"]


class Suite:
    def __init__(self, only=None):
        self.only = only
        self.passed = 0
        self.failed = []
        self.skipped = 0

    def section(self, title):
        print(f"\n--- {title} " + "-" * max(0, 58 - len(title)))

    def run(self, name, fn):
        if self.only and self.only not in name:
            self.skipped += 1
            return
        sys.stdout.write(f"  {name:<52}")
        sys.stdout.flush()
        try:
            t0 = time.time()
            fn()
            print(f"OK ({(time.time() - t0) * 1000:.0f} ms)")
            self.passed += 1
        except Exception as e:
            print(f"FAIL: {e}")
            self.failed.append(f"{name}: {e}")

    def summary(self):
        print(f"\nPassed {self.passed}, failed {len(self.failed)}" +
              (f", skipped {self.skipped}" if self.skipped else ""))
        for f in self.failed:
            print(f"  FAIL  {f}")
        return 1 if self.failed else 0


# ==============================================================================================
# Conformance: the MCP 2026-07-28 wire contract
# ==============================================================================================

def t_healthz(c):
    status, body = c.bare("GET", path=c.url + "/healthz")
    assert status == 200, f"HTTP {status}"
    assert body["status"] == "ok", body
    assert body["tools"] > 0, body
    assert PROTOCOL_VERSION in body["supportedVersions"], body


def t_discover(c):
    r = c.call("server/discover")
    assert PROTOCOL_VERSION in r["supportedVersions"], r
    assert "capabilities" in r and "instructions" in r, sorted(r)
    assert r.get("resultType") == "complete", r.get("resultType")
    assert r["_meta"][META_SERVER_INFO]["name"] == "z80explorer", r.get("_meta")
    assert isinstance(r.get("ttlMs"), int), r.get("ttlMs")
    assert r.get("cacheScope") in ("public", "private"), r.get("cacheScope")


def t_tools_list(c):
    r = c.call("tools/list")
    names = {t["name"] for t in r["tools"]}
    required = {
        "z80_reset", "z80_run", "z80_stop", "z80_now",
        "z80_net_read", "z80_bus_read", "z80_register_read",
        "z80_mem_read", "z80_mem_write", "z80_pin_set",
        "z80_net_find", "z80_net_info", "z80_net_drivers", "z80_trans_info",
        "z80_equation", "z80_equation_tree", "z80_fanout",
        "z80_watchlist_add", "z80_sample_window", "z80_eval_js",
        "z80_save",
    }
    missing = required - names
    assert not missing, f"missing tools: {sorted(missing)}"
    for t in r["tools"]:
        assert "inputSchema" in t, f"{t['name']} has no inputSchema"


def t_tools_list_deterministic(c):
    """Servers SHOULD return tools in a deterministic order, so clients can cache."""
    a = [t["name"] for t in c.call("tools/list")["tools"]]
    b = [t["name"] for t in c.call("tools/list")["tools"]]
    assert a == b, "tools/list order is not stable between calls"


def t_cacheable_fields(c):
    for method in CACHEABLE_METHODS:
        r = c.call(method)
        assert isinstance(r.get("ttlMs"), int), f"{method}: ttlMs is {r.get('ttlMs')!r}"
        assert r.get("cacheScope") in ("public", "private"), f"{method}: cacheScope is {r.get('cacheScope')!r}"


def t_resources_read_cacheable(c):
    """resources/read is a CacheableResult too, and a client's result schema requires both fields.

    It needs a uri argument so it cannot ride the loop above, which is how it came to be the one
    cacheable method missing them.
    """
    r = c.call("resources/read", {"uri": "z80://nets"})
    assert isinstance(r.get("ttlMs"), int), f"ttlMs is {r.get('ttlMs')!r}"
    assert r.get("cacheScope") in ("public", "private"), f"cacheScope is {r.get('cacheScope')!r}"
    assert r.get("resultType") == "complete", r.get("resultType")


def t_result_type_everywhere(c):
    for method in ("server/discover",) + CACHEABLE_METHODS:
        r = c.call(method)
        assert r.get("resultType") == "complete", f"{method}: resultType is {r.get('resultType')!r}"
    r = c.tool("z80_now")
    assert r.get("resultType") == "complete", f"tools/call: resultType is {r.get('resultType')!r}"


def t_meta_absent(c):
    status, _ct, body = c.post("tools/list", meta=None)
    assert status == 400, f"HTTP {status}"
    assert _err(body)["code"] == INVALID_PARAMS, _err(body)


def t_meta_wrong_version(c):
    status, _ct, body = c.post("tools/list",
                               meta={META_VERSION: "2025-06-18", META_CLIENT_CAPS: {}},
                               headers={"MCP-Protocol-Version": "2025-06-18"})
    assert status == 400, f"HTTP {status}"
    e = _err(body)
    assert e["code"] == UNSUPPORTED_PROTOCOL_VERSION, e
    assert PROTOCOL_VERSION in e.get("data", {}).get("supportedVersions", []), e


def t_meta_no_client_caps(c):
    status, _ct, body = c.post("tools/list", meta={META_VERSION: PROTOCOL_VERSION})
    assert status == 400, f"HTTP {status}"
    assert _err(body)["code"] == INVALID_PARAMS, _err(body)


def t_discover_exempt_from_meta(c):
    """A client has to be able to ask what the server is before it can declare anything."""
    status, _ct, body = c.post("server/discover", meta=None)
    assert status == 200, f"HTTP {status}, body {body}"
    assert "result" in body, body


def t_initialize_rejected(c):
    status, _ct, body = c.post("initialize", params={"protocolVersion": "2025-06-18",
                                                     "capabilities": {},
                                                     "clientInfo": {"name": "x", "version": "1"}},
                               meta=None)
    assert status == 400, f"HTTP {status}"
    e = _err(body)
    assert e["code"] == UNSUPPORTED_PROTOCOL_VERSION, e
    assert PROTOCOL_VERSION in e.get("data", {}).get("supportedVersions", []), e


def t_unknown_method(c):
    status, _ct, body = c.post("no/such/method")
    assert status == 404, f"HTTP {status}"
    assert _err(body)["code"] == METHOD_NOT_FOUND, _err(body)


def t_batching_rejected(c):
    status, _ct, body = c.post("tools/list", body=[{"jsonrpc": "2.0", "id": 1, "method": "tools/list"}])
    assert status == 400, f"HTTP {status}"


def t_non_object_body(c):
    status, _ct, _body = c.post("tools/list", body=b'"a bare string"')
    assert status == 400, f"HTTP {status}"


def t_parse_error(c):
    status, _ct, body = c.post("tools/list", body=b"{not json")
    assert status == 400, f"HTTP {status}"
    assert _err(body)["code"] == PARSE_ERROR, _err(body)


def t_notification_202(c):
    status, _ct, body = c.post("notifications/cancelled", rid=None)
    assert status == 202, f"HTTP {status}"
    assert not body, f"a notification must get no body, got {body}"


def t_null_id_rejected(c):
    status, _ct, _body = c.post("tools/list", body={"jsonrpc": "2.0", "id": None, "method": "tools/list",
                                                    "params": {"_meta": c.meta()}})
    assert status == 400, f"HTTP {status}"


def t_bad_jsonrpc_version(c):
    status, _ct, body = c.post("tools/list", body={"jsonrpc": "1.0", "id": 1, "method": "tools/list",
                                                   "params": {"_meta": c.meta()}})
    assert status == 400, f"HTTP {status}"
    assert _err(body)["code"] == INVALID_REQUEST, _err(body)


def t_get_delete_405(c):
    for method in ("GET", "DELETE"):
        status, _body = c.bare(method)
        assert status == 405, f"{method}: HTTP {status}, expected 405"


def t_wrong_content_type(c):
    status, _ct, _body = c.post("tools/list", headers={"Content-Type": "text/plain"})
    assert status == 415, f"HTTP {status}"


def t_content_type_with_charset(c):
    """A charset parameter must not defeat the media-type check."""
    status, _ct, body = c.post("tools/list", headers={"Content-Type": "application/json; charset=utf-8"})
    assert status == 200, f"HTTP {status}, body {body}"


def t_origin_foreign_rejected(c):
    status, _ct, _body = c.post("tools/list", headers={"Origin": "http://evil.example.com"})
    assert status == 403, f"HTTP {status}"


def t_origin_own_accepted(c):
    port = c.url.split(":")[2].split("/")[0]
    status, _ct, body = c.post("tools/list", headers={"Origin": f"http://127.0.0.1:{port}"})
    assert status == 200, f"HTTP {status}, body {body}"


def t_origin_absent_accepted(c):
    status, _ct, body = c.post("tools/list", headers={"Origin": None})
    assert status == 200, f"HTTP {status}, body {body}"


# ---- the standard request headers: validate-if-present ----------------------------------------
# The specification makes MCP-Protocol-Version, Mcp-Method and Mcp-Name REQUIRED and says a server
# MUST reject a request that omits one. This server deliberately accepts an omitted header (and logs
# a warning) so that it interoperates with any client, while still rejecting a header that
# contradicts the body - which is the case the rule exists to protect against. The three
# "absent" tests below pin that deviation down so it stays deliberate.

def t_header_version_mismatch(c):
    status, _ct, body = c.post("tools/list", headers={"MCP-Protocol-Version": "2025-11-25"})
    assert status == 400, f"HTTP {status}"
    assert _err(body)["code"] == HEADER_MISMATCH, _err(body)


def t_header_method_mismatch(c):
    status, _ct, body = c.post("tools/list", headers={"Mcp-Method": "resources/list"})
    assert status == 400, f"HTTP {status}"
    assert _err(body)["code"] == HEADER_MISMATCH, _err(body)


def t_header_name_mismatch(c):
    status, _ct, body = c.post("tools/call", {"name": "z80_now", "arguments": {}},
                               headers={"Mcp-Name": "z80_reset"})
    assert status == 400, f"HTTP {status}"
    assert _err(body)["code"] == HEADER_MISMATCH, _err(body)


def t_header_name_base64(c):
    """Mcp-Name may arrive base64-wrapped; the server must decode before comparing."""
    enc = base64.b64encode(b"z80_now").decode()
    status, _ct, body = c.post("tools/call", {"name": "z80_now", "arguments": {}},
                               headers={"Mcp-Name": f"=?base64?{enc}?="})
    assert status == 200, f"HTTP {status}, body {body}"


def t_header_version_absent_ok(c):
    status, _ct, body = c.post("tools/list", headers={"MCP-Protocol-Version": None})
    assert status == 200, f"deviation changed: HTTP {status}, body {body}"


def t_header_method_absent_ok(c):
    status, _ct, body = c.post("tools/list", headers={"Mcp-Method": None})
    assert status == 200, f"deviation changed: HTTP {status}, body {body}"


def t_header_name_absent_ok(c):
    status, _ct, body = c.post("tools/call", {"name": "z80_now", "arguments": {}},
                               headers={"Mcp-Name": None})
    assert status == 200, f"deviation changed: HTTP {status}, body {body}"


def t_response_mirror_headers(c):
    """The server mirrors method and tool name back, for intermediaries."""
    req = urllib.request.Request(
        c.url, method="POST",
        data=json.dumps({"jsonrpc": "2.0", "id": 99, "method": "tools/call",
                         "params": {"name": "z80_now", "arguments": {}, "_meta": c.meta()}}).encode(),
        headers={"Content-Type": "application/json", "MCP-Protocol-Version": PROTOCOL_VERSION,
                 "Mcp-Method": "tools/call", "Mcp-Name": "z80_now"})
    with urllib.request.urlopen(req, timeout=15) as r:
        assert r.headers.get("Mcp-Method") == "tools/call", r.headers.get("Mcp-Method")
        assert r.headers.get("Mcp-Name") == "z80_now", r.headers.get("Mcp-Name")


# ==============================================================================================
# Tools
# ==============================================================================================

def t_reset(c):
    c.tool_json("z80_reset")
    now = c.tool_json("z80_now")
    assert now["pc"] == 0, now


def t_now(c):
    now = c.tool_json("z80_now")
    for k in ("hc", "pc", "mt", "clk", "running"):
        assert k in now, f"missing {k} in {now}"


def t_net_read(c):
    r = c.tool_json("z80_net_read", {"nets": ["clk", 3, "m1"]})
    assert len(r["results"]) == 3, r
    for row in r["results"]:
        assert row.get("error") or row["value"] in (0, 1, 2), row


def t_bus_read(c):
    c.tool_json("z80_bus_read")


def t_register_read(c):
    c.tool_json("z80_register_read")


def t_trans_read(c):
    r = c.tool_json("z80_trans_read", {"ids": [3514, 2215]})
    assert r, r


def t_net_find(c):
    r = c.tool_json("z80_net_find", {"pattern": "^pla[0-9]+$"})
    assert r["total"] >= 99, r["total"]


def t_net_info(c):
    r = c.tool_json("z80_net_info", {"net": "ixy_d_phase"})
    assert r["id"] == 210, r
    assert r["name"] == "ixy_d_phase", r
    assert len(r["bbox"]) == 4, r


def t_net_info_unknown_suggests(c):
    body = c.tool_raw("z80_net_info", {"net": "ixy_d_phas"})
    r = body.get("result", {})
    assert r.get("isError") or "error" in body, body
    assert "ixy_d_phase" in _first_text(r), f"no near-match suggestion in {_first_text(r)!r}"


def t_net_drivers(c):
    r = c.tool_json("z80_net_drivers", {"net": 210})
    gates = {d.get("gate_name") for d in r["drivers"]}
    assert "ctl_m6" in gates, r


def t_trans_info(c):
    r = c.tool_json("z80_trans_info", {"id": 3514})
    assert len(r["box"]) == 4, r


def t_equation(c):
    r = c.tool_json("z80_equation", {"net": 210})
    assert "ixy_d_phase" in r["expr"], r


def t_equation_tree(c):
    r = c.tool_json("z80_equation_tree", {"net": 210})
    assert r["node_count"] > 10, r
    leaves = {n["name"] for n in r["nodes"].values() if n.get("leaf")}
    assert "m2" in leaves and "t2" in leaves, sorted(leaves)


def t_fanout(c):
    r = c.tool_json("z80_fanout", {"net": 210})
    assert r["gate_count"] == 6, r


def t_fanout_unknown(c):
    body = c.tool_raw("z80_fanout", {"net": "bogus_xyzzy"})
    r = body.get("result", {})
    assert r.get("isError") or "error" in body, body
    txt = _first_text(r)
    assert "bogus_xyzzy" in txt, f"error should name the net: {txt!r}"


def t_mem_roundtrip(c):
    c.tool_json("z80_mem_write", {"addr": 0x100, "bytes": [0xDE, 0xAD, 0xBE, 0xEF]})
    r = c.tool_json("z80_mem_read", {"addr": 0x100, "len": 4})
    got = r["bytes"] if isinstance(r, dict) and "bytes" in r else r
    assert list(got)[:4] == [0xDE, 0xAD, 0xBE, 0xEF], r


def t_io_roundtrip(c):
    c.tool_json("z80_io_write", {"addr": 0x42, "byte": 0x5A})
    c.tool_json("z80_io_read", {"addr": 0x42})


def t_pin_set(c):
    c.tool_json("z80_pin_set", {"pin": "int", "value": 1})
    c.tool_json("z80_pin_set", {"pin": "int", "value": 0})


def t_run_short(c):
    c.tool_json("z80_reset")
    before = c.tool_json("z80_now")["hc"]
    r = c.tool_json("z80_run", {"halfcycles": 10})
    assert r["stopped_by"] == "count", r
    assert c.tool_json("z80_now")["hc"] > before


def t_run_until_cycle(c):
    c.tool_json("z80_reset")
    r = c.tool_json("z80_run", {"halfcycles": 0, "until": {"cycle": 40}, "timeout_ms": 20000})
    assert r["stopped_by"] in ("cycle", "count"), r


def t_break_add_clear(c):
    r = c.tool_json("z80_break_add", {"pc": 0x20})
    c.tool_json("z80_break_clear", {"id": r["id"]} if isinstance(r, dict) and "id" in r else {})


def t_eval_js(c):
    r = c.tool_json("z80_eval_js", {"snippet": "1 + 1"})
    assert "2" in json.dumps(r), r


def t_watchlist_add_and_get(c):
    c.tool_json("z80_watchlist_add", {"nets": ["clk", "m1", "t2"]})
    r = c.tool_json("z80_watchlist_get")
    names = set(r["watchlist"]) if isinstance(r.get("watchlist"), list) else set()
    assert {"clk", "m1", "t2"} <= names, r
    assert r["history_depth"] > 0, r


def t_invalid_args(c):
    status, _ct, body = c.post("tools/call", {"name": "z80_net_info", "arguments": {}})
    assert status == 400 or body.get("result", {}).get("isError"), body


def t_unknown_tool(c):
    status, _ct, body = c.post("tools/call", {"name": "z80_no_such_tool", "arguments": {}})
    assert status == 400, f"HTTP {status}"
    assert _err(body)["code"] == INVALID_PARAMS, _err(body)


def _save_list(c):
    """The save registry as saveList() reports it: {id: available}. Read-only."""
    r = c.tool_json("z80_eval_js", {"snippet": "print(JSON.stringify(saveList()))"})
    return json.loads(r["stdout"])


def t_save_list(c):
    """The registry is discoverable and carries the ids the tool description advertises."""
    items = _save_list(c)
    ids = [i["id"] for i in items]
    assert ids[:4] == ["annotations", "netnames", "colors", "watchlist"], ids
    assert [i for i in ids if i.startswith("waveform-")] == \
           ["waveform-1", "waveform-2", "waveform-3", "waveform-4"], ids
    for i in items:
        assert set(i) == {"id", "name", "files", "available"}, i
        # An available item must name what it would write; an unavailable one has nothing to name
        assert bool(i["files"]) == i["available"], i


def t_save_unknown_id(c):
    """A misspelled id is correctable, so the whole call is refused, naming the valid ids.
    Nothing is written, even though a valid id was also present."""
    for args in ({"items": ["bogus"]}, {"items": ["netnames", "bogus"]}):
        result = c.tool_raw("z80_save", args).get("result", {})
        assert result.get("isError"), (args, result)
        text = _first_text(result)
        assert "annotations" in text and "netnames" in text, text


def t_save_bad_items(c):
    """A malformed 'items' must not fall back to saving everything - this tool rewrites files."""
    for args in ({"items": "netnames"}, {"items": []}, {"items": [7]}):
        result = c.tool_raw("z80_save", args).get("result", {})
        assert result.get("isError"), (args, result)


def t_save_all(c):
    """No arguments saves every available item. DESTRUCTIVE: rewrites the real user data files."""
    r = c.tool_json("z80_save")
    assert r["failed"] == [], r
    ids = {s["id"] for s in r["saved"]} | {s["id"] for s in r["skipped"]}
    assert {"annotations", "netnames", "colors", "watchlist"} <= ids, r
    for s in r["saved"]:
        assert s["files"], s
    for s in r["skipped"]:
        assert s["reason"], s


def t_save_subset(c):
    """DESTRUCTIVE: rewrites netnames.js and tips.json."""
    r = c.tool_json("z80_save", {"items": ["netnames"]})
    assert r["failed"] == [] and r["skipped"] == [], r
    assert [s["id"] for s in r["saved"]] == ["netnames"], r
    # Net names and their comments are two files that must always be written together
    assert len(r["saved"][0]["files"]) == 2, r


def t_save_unavailable_id(c):
    """A waveform view that is not open is a real state, not a mistake: it is reported per item
    and must not stop the other items in the same call. DESTRUCTIVE: rewrites watchlist.json."""
    avail = {i["id"]: i["available"] for i in _save_list(c)}
    target = next((i for i in ("waveform-4", "waveform-3", "waveform-2", "waveform-1")
                   if not avail[i]), None)
    assert target, "all four waveform views are open; close one so this case has something to test"

    r = c.tool_json("z80_save", {"items": ["watchlist", target]})
    assert [s["id"] for s in r["saved"]] == ["watchlist"], r
    assert [f["id"] for f in r["failed"]] == [target], r
    assert "nothing to save yet" in r["failed"][0]["reason"], r


# ---- regression cases for fixed bugs -----------------------------------------------------------

def t_sample_window_exact_count(c):
    """Regression: the window used to return halfcycles+9 samples, the last of every column 3.

    Reset burns 8 half-cycles and the upper bound was inclusive, so the reply carried the reset
    propagation plus one read past the last written sample, where ClassWatch::at returns its
    no-data sentinel.
    """
    n = 20
    r = c.tool_json("z80_sample_window", {"nets": ["clk", "m1"], "halfcycles": n, "reset": True},
                    timeout=60)
    for name, col in r["samples"].items():
        assert len(col) == n, f"{name}: got {len(col)} samples for halfcycles={n}"
        assert 3 not in col, f"{name}: no-data sentinel 3 present at index {col.index(3)}"
    assert len(set(r["samples"]["clk"])) == 2, "clk should toggle"


def t_sample_window_numeric_nets(c):
    """A net carrying no name is captured by its number, as a full column.

    ClassWatch resolves a watch key by name, so a numeric reference used to be dropped silently and
    the column came back as a single current value instead of the window. const66_gen (795) is
    named now, so this uses a still-unnamed ALU bus control to keep testing the numeric path.
    """
    n = 20
    r = c.tool_json("z80_sample_window", {"nets": [604, "684", "clk"], "halfcycles": n,
                                          "reset": True}, timeout=60)
    cols = r["samples"]
    assert set(cols) == {"604", "684", "clk"}, f"unexpected columns: {sorted(cols)}"
    for name, col in cols.items():
        assert len(col) == n, f"{name}: got {len(col)} samples for halfcycles={n}"


def t_sample_window_unknown_net(c):
    """An entry naming nothing fails the call instead of yielding a silently wrong column."""
    body = c.tool_raw("z80_sample_window", {"nets": ["clk", "no_such_net_xyz"],
                                            "halfcycles": 4, "reset": True}, timeout=60)
    res = body.get("result", {})
    assert res.get("isError") or ("error" in body), f"expected an error, got {body}"
    text = json.dumps(body)
    assert "no_such_net_xyz" in text, f"the rejection should name the offending entry: {text}"
    # An out-of-range number is rejected the same way, rather than being watched as net 0.
    body = c.tool_raw("z80_sample_window", {"nets": [99999], "halfcycles": 4, "reset": True},
                      timeout=60)
    res = body.get("result", {})
    assert res.get("isError") or ("error" in body), f"expected an error, got {body}"


def t_sample_window_hc_and_mt(c):
    r = c.tool_json("z80_sample_window", {"nets": ["clk"], "halfcycles": 12, "reset": True}, timeout=60)
    hc, mt = r["hc"], r["mt"]
    assert len(hc) == len(mt) == len(r["samples"]["clk"]), (len(hc), len(mt))
    assert hc == sorted(hc) and len(set(hc)) == len(hc), "hc must be strictly increasing"
    assert any(s.startswith("M1T") for s in mt), mt


def t_sample_window_net_210(c):
    """The ixy_d_phase window: high from M2T2 through the end of M3, for `ini`."""
    c.tool_json("z80_mem_write", {"addr": 0, "bytes": [0xED, 0xA2, 0x76]})
    r = c.tool_json("z80_sample_window",
                    {"nets": ["ixy_d_phase"], "halfcycles": 46, "reset": True}, timeout=60)
    hi = [m for m, v in zip(r["mt"], r["samples"]["ixy_d_phase"]) if v == 1]
    assert hi, "ixy_d_phase never asserted for ini"
    assert hi[0] == "M2T2", f"first assertion at {hi[0]}, expected M2T2"
    assert any(m.startswith("M3T") for m in hi), hi


def t_waveform_window_no_sentinel(c):
    c.tool_json("z80_watchlist_add", {"nets": ["clk"]})
    c.tool_json("z80_reset")
    c.tool_json("z80_run", {"halfcycles": 20})
    now = c.tool_json("z80_now")["hc"]
    r = c.tool_json("z80_waveform_window", {"nets": ["clk"], "from_hc": now - 10, "to_hc": now})
    col = r["samples"]["clk"] if "samples" in r else r
    assert 3 not in col, f"trailing no-data sentinel present: {col}"


def t_view_set_fractional_zoom(c):
    c.tool_json("z80_view_set", {"x": 3500, "y": 4000, "zoom": 2.5})


def t_die_info(c):
    r = c.tool_json("z80_die_info")
    assert r["image"]["width"] == 4700 and r["image"]["height"] == 5000, r["image"]
    assert len(r["layers"]) >= 20, len(r["layers"])
    assert {"index", "key", "name"} <= set(r["layers"][0]), r["layers"][0]
    assert len(r["net_modes"]) == 4 and len(r["transistor_modes"]) == 4, r
    assert "3" in {str(k) for k in r["net_values"]}, r["net_values"]
    assert "segdefs" in json.dumps(r["coords"]).lower(), r["coords"]


# ---- batched query forms -----------------------------------------------------------------------

def t_net_info_batch(c):
    nets = [156, 202, 230, 238, 243, 253, 377, 400, 403, 404]
    r = c.tool_json("z80_net_info", {"nets": nets})
    assert len(r["results"]) == len(nets), len(r["results"])
    assert r["results"][0]["id"] == 156, r["results"][0]


def t_net_info_batch_partial_failure(c):
    r = c.tool_json("z80_net_info", {"nets": [210, "bogus_xyzzy", 215]})
    rows = r["results"]
    assert len(rows) == 3, rows
    assert rows[0]["id"] == 210 and rows[2]["id"] == 215, rows
    assert rows[1].get("error"), "a bad entry must carry its own error, not fail the batch"


def t_net_drivers_batch(c):
    r = c.tool_json("z80_net_drivers", {"nets": [210, 241, 1332]})
    assert len(r["results"]) == 3, r


def t_trans_info_batch(c):
    r = c.tool_json("z80_trans_info", {"ids": [3514, 2215, 2763, 2766, 2771]})
    assert len(r["results"]) == 5, r


def t_equation_batch(c):
    r = c.tool_json("z80_equation", {"nets": [210, 215]}, timeout=120)
    assert len(r["results"]) == 2, r


def t_batch_cap(c):
    body = c.tool_raw("z80_net_info", {"nets": list(range(1, 400))})
    r = body.get("result", {})
    assert r.get("isError") or "error" in body, "an oversized batch should be refused"


# ---- the die renderer --------------------------------------------------------------------------

def t_view_render_inline(c):
    r = c.tool("z80_view_render", {"x": 3330, "y": 3660, "w": 370, "h": 840, "scale": 2,
                                   "layers": ["metal", "polysilicon"],
                                   "nets": {"825": "#ff3c3c", "826": "#3ce65a", "828": "#5096ff"}},
               timeout=120)
    imgs = [b for b in r.get("content", []) if b.get("type") == "image"]
    assert imgs, f"no image block in {[b.get('type') for b in r.get('content', [])]}"
    assert imgs[0]["mimeType"] == "image/png", imgs[0]["mimeType"]
    raw = base64.b64decode(imgs[0]["data"])
    assert raw[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    meta = r["structuredContent"]
    assert meta["width"] == 740 and meta["height"] == 1680, (meta["width"], meta["height"])


def t_view_render_file_above_cap(c):
    r = c.tool("z80_view_render", {"x": 0, "y": 0, "w": 4700, "h": 5000, "scale": 1,
                                   "inline_max_bytes": 4096}, timeout=180)
    meta = r["structuredContent"]
    assert meta.get("path"), f"expected a file path above the cap, got {meta}"
    assert not [b for b in r.get("content", []) if b.get("type") == "image"], \
        "must not inline an image above the cap"


def t_view_render_all_layers(c):
    layers = [l["name"] for l in c.tool_json("z80_die_info")["layers"]]
    for name in layers:
        r = c.tool("z80_view_render", {"x": 2000, "y": 2000, "w": 64, "h": 64, "layers": [name]},
                   timeout=60)
        assert not r.get("isError"), f"layer {name}: {_first_text(r)}"


def t_view_render_bad_rect(c):
    body = c.tool_raw("z80_view_render", {"x": -5, "y": 0, "w": 0, "h": 0})
    r = body.get("result", {})
    assert r.get("isError") or "error" in body, "a degenerate rect should be refused"


# ==============================================================================================
# Concurrency
# ==============================================================================================

def t_concurrent_reads(c, n=100):
    def one(_):
        return c.tool_json("z80_net_read", {"nets": ["clk"]})
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
        for r in ex.map(one, range(n)):
            assert r["results"][0]["value"] in (0, 1, 2), r


def t_tool_busy_conflict(c):
    """A long run must make a second non-reentrant tool return 409 ToolBusy, not corrupt state."""
    done = []

    def long_run():
        # A large half-cycle count, not an `until` condition: until.cycle is a 16-bit value, so it
        # cannot express "run for long enough that a second call arrives mid-flight".
        done.append(c.tool_json("z80_run", {"halfcycles": 400000, "timeout_ms": 8000}, timeout=30))

    th = threading.Thread(target=long_run)
    th.start()
    try:
        deadline = time.time() + 5
        seen = None
        while time.time() < deadline and seen is None:
            status, _ct, body = c.post("tools/call", {"name": "z80_reset", "arguments": {}})
            if status == 409:
                seen = body
        assert seen is not None, "never observed a 409 ToolBusy while a run was in flight"
    finally:
        c.tool_json("z80_stop")
        th.join(timeout=30)


def t_stop_while_running(c):
    def stopper():
        c.tool_json("z80_stop")

    th = threading.Thread(target=stopper)
    th.start()
    r = c.tool_json("z80_run", {"halfcycles": 400000, "timeout_ms": 15000}, timeout=40)
    th.join(timeout=20)
    assert r["stopped_by"] in ("stop", "timeout", "count"), r


# ==============================================================================================

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--quick", action="store_true", help="skip the concurrency cases")
    ap.add_argument("--destructive", action="store_true",
                    help="also run the z80_save cases that overwrite your real user data files")
    ap.add_argument("--only", help="run only tests whose name contains this substring")
    args = ap.parse_args()

    url = f"http://{args.host}:{args.port}/mcp"
    c = McpClient(url)
    print(f"Z80 Explorer MCP suite -> {url}  (protocol {PROTOCOL_VERSION})")
    try:
        c.bare("GET", path=url + "/healthz")
    except Exception as e:
        print(f"\nCannot reach the server: {e}")
        print("Start Z80Explorer.exe and make sure the MCP server is enabled in Edit > Settings.")
        return 2

    s = Suite(only=args.only)

    s.section("Conformance: discovery and results")
    s.run("healthz", lambda: t_healthz(c))
    s.run("server/discover", lambda: t_discover(c))
    s.run("tools/list", lambda: t_tools_list(c))
    s.run("tools/list is deterministic", lambda: t_tools_list_deterministic(c))
    s.run("ttlMs + cacheScope on cacheable results", lambda: t_cacheable_fields(c))
    s.run("resources/read is cacheable too", lambda: t_resources_read_cacheable(c))
    s.run("resultType on every result", lambda: t_result_type_everywhere(c))

    s.section("Conformance: _meta and versioning")
    s.run("_meta absent -> -32602", lambda: t_meta_absent(c))
    s.run("_meta wrong version -> -32022", lambda: t_meta_wrong_version(c))
    s.run("_meta without clientCapabilities -> -32602", lambda: t_meta_no_client_caps(c))
    s.run("server/discover exempt from _meta", lambda: t_discover_exempt_from_meta(c))
    s.run("initialize -> -32022", lambda: t_initialize_rejected(c))

    s.section("Conformance: JSON-RPC and transport")
    s.run("unknown method -> 404 -32601", lambda: t_unknown_method(c))
    s.run("batching rejected", lambda: t_batching_rejected(c))
    s.run("non-object body rejected", lambda: t_non_object_body(c))
    s.run("malformed JSON -> -32700", lambda: t_parse_error(c))
    s.run("notification -> 202, no body", lambda: t_notification_202(c))
    s.run("null id rejected", lambda: t_null_id_rejected(c))
    s.run("bad jsonrpc version rejected", lambda: t_bad_jsonrpc_version(c))
    s.run("GET and DELETE -> 405", lambda: t_get_delete_405(c))
    s.run("non-JSON Content-Type -> 415", lambda: t_wrong_content_type(c))
    s.run("Content-Type with charset accepted", lambda: t_content_type_with_charset(c))
    s.run("foreign Origin -> 403", lambda: t_origin_foreign_rejected(c))
    s.run("own Origin accepted", lambda: t_origin_own_accepted(c))
    s.run("absent Origin accepted", lambda: t_origin_absent_accepted(c))

    s.section("Conformance: standard request headers (validate-if-present)")
    s.run("version header mismatch -> -32020", lambda: t_header_version_mismatch(c))
    s.run("Mcp-Method mismatch -> -32020", lambda: t_header_method_mismatch(c))
    s.run("Mcp-Name mismatch -> -32020", lambda: t_header_name_mismatch(c))
    s.run("Mcp-Name base64 sentinel decoded", lambda: t_header_name_base64(c))
    s.run("version header absent accepted", lambda: t_header_version_absent_ok(c))
    s.run("Mcp-Method absent accepted", lambda: t_header_method_absent_ok(c))
    s.run("Mcp-Name absent accepted", lambda: t_header_name_absent_ok(c))
    s.run("response mirrors Mcp-Method / Mcp-Name", lambda: t_response_mirror_headers(c))

    s.section("Tools: state and simulation")
    s.run("z80_reset", lambda: t_reset(c))
    s.run("z80_now", lambda: t_now(c))
    s.run("z80_net_read", lambda: t_net_read(c))
    s.run("z80_bus_read", lambda: t_bus_read(c))
    s.run("z80_register_read", lambda: t_register_read(c))
    s.run("z80_trans_read", lambda: t_trans_read(c))
    s.run("z80_mem_read/write roundtrip", lambda: t_mem_roundtrip(c))
    s.run("z80_io_read/write roundtrip", lambda: t_io_roundtrip(c))
    s.run("z80_pin_set", lambda: t_pin_set(c))
    s.run("z80_run (count)", lambda: t_run_short(c))
    s.run("z80_run (until cycle)", lambda: t_run_until_cycle(c))
    s.run("z80_break add + clear", lambda: t_break_add_clear(c))
    s.run("z80_eval_js", lambda: t_eval_js(c))

    s.section("Tools: netlist queries")
    s.run("z80_net_find", lambda: t_net_find(c))
    s.run("z80_net_info", lambda: t_net_info(c))
    s.run("z80_net_info unknown suggests near match", lambda: t_net_info_unknown_suggests(c))
    s.run("z80_net_drivers", lambda: t_net_drivers(c))
    s.run("z80_trans_info", lambda: t_trans_info(c))
    s.run("z80_equation", lambda: t_equation(c))
    s.run("z80_equation_tree", lambda: t_equation_tree(c))
    s.run("z80_fanout", lambda: t_fanout(c))
    s.run("z80_fanout unknown net errors", lambda: t_fanout_unknown(c))
    s.run("invalid args rejected", lambda: t_invalid_args(c))
    s.run("unknown tool -> -32602", lambda: t_unknown_tool(c))

    s.section("Tools: saving user data")
    s.run("save registry is discoverable", lambda: t_save_list(c))
    s.run("z80_save unknown id refused", lambda: t_save_unknown_id(c))
    s.run("z80_save malformed items refused", lambda: t_save_bad_items(c))
    if args.destructive:
        s.run("z80_save every available item", lambda: t_save_all(c))
        s.run("z80_save a single item", lambda: t_save_subset(c))
        s.run("z80_save unavailable id reported per item", lambda: t_save_unavailable_id(c))

    s.section("Tools: batched query forms")
    s.run("z80_net_info batch", lambda: t_net_info_batch(c))
    s.run("z80_net_info batch partial failure", lambda: t_net_info_batch_partial_failure(c))
    s.run("z80_net_drivers batch", lambda: t_net_drivers_batch(c))
    s.run("z80_trans_info batch", lambda: t_trans_info_batch(c))
    s.run("z80_equation batch", lambda: t_equation_batch(c))
    s.run("oversized batch refused", lambda: t_batch_cap(c))

    s.section("Tools: waveforms and sampling")
    s.run("z80_watchlist_add + z80_watchlist_get", lambda: t_watchlist_add_and_get(c))
    s.run("z80_sample_window exact count, no sentinel", lambda: t_sample_window_exact_count(c))
    s.run("z80_sample_window numeric (unnamed) nets", lambda: t_sample_window_numeric_nets(c))
    s.run("z80_sample_window unknown net refused", lambda: t_sample_window_unknown_net(c))
    s.run("z80_sample_window hc + mt arrays", lambda: t_sample_window_hc_and_mt(c))
    s.run("z80_sample_window ixy_d_phase window", lambda: t_sample_window_net_210(c))
    s.run("z80_waveform_window no sentinel", lambda: t_waveform_window_no_sentinel(c))

    s.section("Tools: logic tree and the die view")
    s.run("z80_die_info", lambda: t_die_info(c))
    s.run("z80_view_set fractional zoom", lambda: t_view_set_fractional_zoom(c))
    s.run("z80_view_render inline PNG", lambda: t_view_render_inline(c))
    s.run("z80_view_render file above cap", lambda: t_view_render_file_above_cap(c))
    s.run("z80_view_render every layer", lambda: t_view_render_all_layers(c))
    s.run("z80_view_render degenerate rect refused", lambda: t_view_render_bad_rect(c))

    if not args.quick:
        s.section("Concurrency")
        s.run("concurrent net reads", lambda: t_concurrent_reads(c, 100))
        s.run("409 ToolBusy while a run is in flight", lambda: t_tool_busy_conflict(c))
        s.run("stop while running", lambda: t_stop_while_running(c))

    return s.summary()


if __name__ == "__main__":
    sys.exit(main())
