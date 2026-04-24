#include "ClassMcpTools.h"
#include "ClassAnnotate.h"
#include "ClassController.h"
#include "ClassMcpThreading.h"
#include "ClassNetlist.h"
#include "ClassRenderer.h"
#include "ClassScript.h"
#include "ClassSpatial.h"
#include "ClassTrickbox.h"
#include "ClassVisual.h"
#include "ClassWatch.h"
#include "z80state.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPixmap>
#include <QRegularExpression>
#include <QTimer>
#include <QWidget>
#include <climits>

ClassMcpTools::ClassMcpTools(ClassSpatial *spatial, ClassRenderer *renderer, QObject *parent)
    : QObject(parent), m_spatial(spatial), m_renderer(renderer)
{
}

// ===========================================================================
// Helpers
// ===========================================================================

int ClassMcpTools::intArg(const QJsonObject &a, const QString &key, int def)
{
    QJsonValue v = a.value(key);
    if (v.isDouble()) return v.toInt(def);
    if (v.isString()) { bool ok = false; int n = v.toString().toInt(&ok, 0); return ok ? n : def; }
    return def;
}

uint ClassMcpTools::uintArg(const QJsonObject &a, const QString &key, uint def)
{
    QJsonValue v = a.value(key);
    if (v.isDouble()) return uint(v.toDouble(double(def)));
    if (v.isString()) { bool ok = false; uint n = v.toString().toUInt(&ok, 0); return ok ? n : def; }
    return def;
}

bool ClassMcpTools::boolArg(const QJsonObject &a, const QString &key, bool def)
{
    QJsonValue v = a.value(key);
    if (v.isBool()) return v.toBool();
    return def;
}

QString ClassMcpTools::strArg(const QJsonObject &a, const QString &key, const QString &def)
{
    QJsonValue v = a.value(key);
    return v.isString() ? v.toString() : def;
}

QJsonValue ClassMcpTools::textResult(const QString &text, bool isError)
{
    QJsonObject item; item["type"] = "text"; item["text"] = text;
    QJsonArray content; content.append(item);
    QJsonObject r; r["content"] = content; r["isError"] = isError;
    return r;
}

QJsonValue ClassMcpTools::textResult(const QJsonValue &jsonPayload)
{
    // Serialize the JSON payload as text content for clients that render plain text.
    QJsonDocument doc(jsonPayload.isObject() ? jsonPayload.toObject() : QJsonObject{{"value", jsonPayload}});
    return textResult(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

QJsonValue ClassMcpTools::imageResult(const QByteArray &pngBytes, const QString &caption)
{
    QJsonArray content;
    QJsonObject img;
    img["type"]     = "image";
    img["data"]     = QString::fromLatin1(pngBytes.toBase64());
    img["mimeType"] = "image/png";
    content.append(img);
    if (!caption.isEmpty())
    {
        QJsonObject text;
        text["type"] = "text";
        text["text"] = caption;
        content.append(text);
    }
    QJsonObject r;
    r["content"] = content;
    r["isError"] = false;
    return r;
}

QJsonValue ClassMcpTools::mixedResult(const QByteArray &pngBytes, const QJsonValue &jsonPayload)
{
    QJsonArray content;
    QJsonObject img;
    img["type"]     = "image";
    img["data"]     = QString::fromLatin1(pngBytes.toBase64());
    img["mimeType"] = "image/png";
    content.append(img);

    QJsonObject text;
    text["type"] = "text";
    QJsonDocument doc(jsonPayload.isObject() ? jsonPayload.toObject() : QJsonObject{});
    text["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    content.append(text);

    QJsonObject r;
    r["content"] = content;
    r["isError"] = false;
    return r;
}

net_t ClassMcpTools::resolveNet(const QJsonValue &v) const
{
    if (v.isDouble())
        return net_t(v.toInt());
    if (v.isString())
    {
        QString s = v.toString();
        bool ok = false;
        int n = s.toInt(&ok, 0);
        if (ok) return net_t(n);
        return ::controller.getNetlist().get(s);
    }
    return 0;
}

pin_t ClassMcpTools::readBitByName(const QString &name) const
{
#if USE_AVX2_SIM
    return ::controller.getSimZ80().readBit(name);
#else
    return ::controller.getNetlist().readBit(name);
#endif
}

pin_t ClassMcpTools::readBitByNum(net_t n) const
{
#if USE_AVX2_SIM
    return ::controller.getSimZ80().readBit(n);
#else
    return ::controller.getNetlist().readBit(n);
#endif
}

// ===========================================================================
// Dispatch
// ===========================================================================

void ClassMcpTools::registerTool(const ToolDef &t)
{
    m_tools.append(t);
}

QJsonArray ClassMcpTools::toolsList() const
{
    QJsonArray out;
    for (const ToolDef &t : m_tools)
    {
        QJsonObject o;
        o["name"]        = t.name;
        o["description"] = t.description;
        o["inputSchema"] = t.inputSchema;
        out.append(o);
    }
    return out;
}

QJsonValue ClassMcpTools::invoke(const QString &name, const QJsonObject &args, QString &err)
{
    for (const ToolDef &t : m_tools)
    {
        if (t.name == name)
        {
            QElapsedTimer timer; timer.start();
            QJsonValue r = t.handler(args, err);
            if (timer.elapsed() > 2000)
                qDebug() << "MCP tool" << name << "took" << timer.elapsed() << "ms";
            return r;
        }
    }
    err = QString("Unknown tool: %1").arg(name);
    return {};
}

// ===========================================================================
// Schema fragments reused across tools
// ===========================================================================

static QJsonObject schemaString(const QString &desc = {})
{
    QJsonObject s; s["type"] = "string";
    if (!desc.isEmpty()) s["description"] = desc;
    return s;
}
static QJsonObject schemaInt(const QString &desc = {})
{
    QJsonObject s; s["type"] = "integer";
    if (!desc.isEmpty()) s["description"] = desc;
    return s;
}
static QJsonObject schemaBool(const QString &desc = {})
{
    QJsonObject s; s["type"] = "boolean";
    if (!desc.isEmpty()) s["description"] = desc;
    return s;
}
static QJsonObject schemaArray(const QJsonObject &items, const QString &desc = {})
{
    QJsonObject s; s["type"] = "array"; s["items"] = items;
    if (!desc.isEmpty()) s["description"] = desc;
    return s;
}
static QJsonObject schemaObject(const QJsonObject &props, const QJsonArray &required = {})
{
    QJsonObject s; s["type"] = "object"; s["properties"] = props;
    if (!required.isEmpty()) s["required"] = required;
    return s;
}

// ===========================================================================
// Register all 26 tools
// ===========================================================================

void ClassMcpTools::registerDefaults()
{
    m_tools.clear();

    // --- Execution control ---------------------------------------------------

    registerTool({
        "z80_load_hex",
        "Load an Intel HEX file into simulated RAM. Pass clear=false to merge without wiping.",
        schemaObject({
            {"path",  schemaString("Path to the .hex file")},
            {"clear", schemaBool("If true (default), clears RAM before load")},
        }, {"path"}),
        [this](const QJsonObject &a, QString &err) { return hndLoadHex(a, err); }
    });

    registerTool({
        "z80_reset",
        "Reset the Z80 chip. Clears the watch history and re-initialises the netlist.",
        schemaObject({}, {}),
        [this](const QJsonObject &a, QString &err) { return hndReset(a, err); }
    });

    registerTool({
        "z80_run",
        "Run the simulation for N half-cycles, or until one of the `until` conditions fires. "
        "`until` object fields: {pc:int, cycle:int, net:(string|int), value:0|1}. "
        "`timeout_ms` bounds wall-clock wait (default 30000). Returns stopped_by: 'cycle'|'pc'|'net'|'count'|'timeout'|'stop'.",
        schemaObject({
            {"halfcycles", schemaInt("Maximum half-cycles to run. 0 = until condition or stop.")},
            {"until",      schemaObject({
                              {"pc",    schemaInt()},
                              {"cycle", schemaInt()},
                              {"net",   schemaString()},
                              {"value", schemaInt()},
                          })},
            {"timeout_ms", schemaInt("Wall-clock timeout in milliseconds; default 30000")},
        }, {}),
        [this](const QJsonObject &a, QString &err) { return hndRun(a, err); }
    });

    registerTool({
        "z80_stop",
        "Stop the running simulation immediately.",
        schemaObject({}, {}),
        [this](const QJsonObject &a, QString &err) { return hndStop(a, err); }
    });

    registerTool({
        "z80_now",
        "Return the current sim status: half-cycle, PC, MT state, clk level, running flag.",
        schemaObject({}, {}),
        [this](const QJsonObject &a, QString &err) { return hndNow(a, err); }
    });

    // --- State reads ---------------------------------------------------------

    registerTool({
        "z80_net_read",
        "Batch-read the current logic value of the listed nets. Each entry may be a net name or numeric id. Returns {id, name, value} where value is 0, 1, or 2 (hi-Z).",
        schemaObject({
            {"nets", schemaArray(schemaObject({}, {}), "Names or ids")},
        }, {"nets"}),
        [this](const QJsonObject &a, QString &err) { return hndNetRead(a, err); }
    });

    registerTool({
        "z80_bus_read",
        "Read the external bus pins: AB, DB, /M1, /MREQ, /IORQ, /RD, /WR, /RFSH, /HALT, /BUSAK.",
        schemaObject({}, {}),
        [this](const QJsonObject &a, QString &err) { return hndBusRead(a, err); }
    });

    registerTool({
        "z80_register_read",
        "Read the Z80 CPU registers (PC, IR, WZ, AF, HL, DE, BC, IX, IY, SP, I, R) and alternates.",
        schemaObject({}, {}),
        [this](const QJsonObject &a, QString &err) { return hndRegisterRead(a, err); }
    });

    registerTool({
        "z80_trans_read",
        "Batch-read transistor state. Returns {id, gate, source, drain, on} for each id.",
        schemaObject({
            {"ids", schemaArray(schemaInt(), "Transistor ids")},
        }, {"ids"}),
        [this](const QJsonObject &a, QString &err) { return hndTransRead(a, err); }
    });

    registerTool({
        "z80_waveform_window",
        "Return the sampled value of each net across a half-cycle range. Nets must be in the watchlist. Missing from_hc/to_hc default to the full stored window.",
        schemaObject({
            {"nets",    schemaArray(schemaString(), "Net names")},
            {"from_hc", schemaInt()},
            {"to_hc",   schemaInt()},
        }, {"nets"}),
        [this](const QJsonObject &a, QString &err) { return hndWaveformWindow(a, err); }
    });

    // --- Metadata / discovery ------------------------------------------------

    registerTool({
        "z80_net_find",
        "Find nets by name (regex pattern). Returns matching {id, name}.",
        schemaObject({
            {"pattern", schemaString("Regular expression to match against net names")},
        }, {"pattern"}),
        [this](const QJsonObject &a, QString &err) { return hndNetFind(a, err); }
    });

    registerTool({
        "z80_net_info",
        "Get structural information about a net: name, driving/driven transistors, pullup, bounding box.",
        schemaObject({
            {"net", schemaString("Net name or numeric id")},
        }, {"net"}),
        [this](const QJsonObject &a, QString &err) { return hndNetInfo(a, err); }
    });

    registerTool({
        "z80_trans_info",
        "Get information about a transistor: gate/source/drain nets, box, current on/off state.",
        schemaObject({
            {"id", schemaInt("Transistor id")},
        }, {"id"}),
        [this](const QJsonObject &a, QString &err) { return hndTransInfo(a, err); }
    });

    registerTool({
        "z80_equation",
        "Return the logic equation driving a net (optimised expression tree).",
        schemaObject({
            {"net", schemaString("Net name or numeric id")},
        }, {"net"}),
        [this](const QJsonObject &a, QString &err) { return hndEquation(a, err); }
    });

    // --- Memory / IO ---------------------------------------------------------

    registerTool({
        "z80_mem_read",
        "Read a range of bytes from simulated RAM.",
        schemaObject({
            {"addr", schemaInt()},
            {"len",  schemaInt()},
        }, {"addr", "len"}),
        [this](const QJsonObject &a, QString &err) { return hndMemRead(a, err); }
    });

    registerTool({
        "z80_mem_write",
        "Write a sequence of bytes into simulated RAM.",
        schemaObject({
            {"addr",  schemaInt()},
            {"bytes", schemaArray(schemaInt())},
        }, {"addr", "bytes"}),
        [this](const QJsonObject &a, QString &err) { return hndMemWrite(a, err); }
    });

    registerTool({
        "z80_io_read",
        "Read a single byte from the simulated IO space.",
        schemaObject({{"addr", schemaInt()}}, {"addr"}),
        [this](const QJsonObject &a, QString &err) { return hndIoRead(a, err); }
    });

    registerTool({
        "z80_io_write",
        "Write a single byte to the simulated IO space.",
        schemaObject({{"addr", schemaInt()}, {"byte", schemaInt()}}, {"addr", "byte"}),
        [this](const QJsonObject &a, QString &err) { return hndIoWrite(a, err); }
    });

    // --- Pin control ---------------------------------------------------------

    registerTool({
        "z80_pin_set",
        "Drive an input pin (INT/NMI/BUSRQ/WAIT/RESET) to a value immediately.",
        schemaObject({
            {"pin",   schemaString("One of INT, NMI, BUSRQ, WAIT, RESET")},
            {"value", schemaInt("0 asserted (active-low), 1 de-asserted, 2 hi-Z if supported")},
        }, {"pin", "value"}),
        [this](const QJsonObject &a, QString &err) { return hndPinSet(a, err); }
    });

    registerTool({
        "z80_pin_set_at",
        "Schedule a pin assertion at a specific half-cycle, held for N half-cycles.",
        schemaObject({
            {"pin",   schemaString()},
            {"at_hc", schemaInt()},
            {"hold",  schemaInt()},
        }, {"pin", "at_hc", "hold"}),
        [this](const QJsonObject &a, QString &err) { return hndPinSetAt(a, err); }
    });

    registerTool({
        "z80_pin_set_at_pc",
        "Schedule a pin assertion when the PC reaches a given address, held for N half-cycles.",
        schemaObject({
            {"pin",  schemaString()},
            {"pc",   schemaInt()},
            {"hold", schemaInt()},
        }, {"pin", "pc", "hold"}),
        [this](const QJsonObject &a, QString &err) { return hndPinSetAtPc(a, err); }
    });

    // --- Breakpoints ---------------------------------------------------------

    registerTool({
        "z80_break_add",
        "Register a breakpoint. Provide exactly one of {pc, cycle, net+value}.",
        schemaObject({
            {"pc",    schemaInt()},
            {"cycle", schemaInt()},
            {"net",   schemaString()},
            {"value", schemaInt()},
        }, {}),
        [this](const QJsonObject &a, QString &err) { return hndBreakAdd(a, err); }
    });

    registerTool({
        "z80_break_clear",
        "Clear a single breakpoint by id, or all if id is omitted.",
        schemaObject({{"id", schemaInt()}}, {}),
        [this](const QJsonObject &a, QString &err) { return hndBreakClear(a, err); }
    });

    // --- Spatial queries -----------------------------------------------------

    registerTool({
        "z80_region_of",
        "Return the functional block (if any) containing a net or transistor, plus nearby annotations.",
        schemaObject({
            {"net",   schemaString()},
            {"trans", schemaInt()},
        }, {}),
        [this](const QJsonObject &a, QString &err) { return hndRegionOf(a, err); }
    });

    registerTool({
        "z80_nets_near",
        "Return nets whose bounding-box centre lies within `radius` pixels of (x, y) on the 4700x5000 die.",
        schemaObject({
            {"x",      schemaInt()},
            {"y",      schemaInt()},
            {"radius", schemaInt()},
        }, {"x", "y", "radius"}),
        [this](const QJsonObject &a, QString &err) { return hndNetsNear(a, err); }
    });

    registerTool({
        "z80_trans_near",
        "Return transistors whose box centre lies within `radius` pixels of (x, y).",
        schemaObject({
            {"x",      schemaInt()},
            {"y",      schemaInt()},
            {"radius", schemaInt()},
        }, {"x", "y", "radius"}),
        [this](const QJsonObject &a, QString &err) { return hndTransNear(a, err); }
    });

    registerTool({
        "z80_bounding_box",
        "Return the minimum bounding rectangle of a set of nets or transistors plus a compactness score (area sum / bbox area).",
        schemaObject({
            {"ids",  schemaArray(schemaInt())},
            {"type", schemaString("'net' or 'trans'")},
        }, {"ids", "type"}),
        [this](const QJsonObject &a, QString &err) { return hndBoundingBox(a, err); }
    });

    // --- Visual / rendering --------------------------------------------------

    registerTool({
        "z80_render_region",
        "Render an arbitrary region of the die to a PNG that the caller (LLM) can SEE. "
        "center can be {x,y}, {net}, or {trans}. zoom is relative (1.0 = default). "
        "layers lists chip layers to composite (diffusion/polysilicon/metal/vias/buried/ions). "
        "highlight_nets / highlight_trans paint extra overlays. Returns image + metadata.",
        schemaObject({
            {"center",          schemaObject({{"x", schemaInt()}, {"y", schemaInt()},
                                              {"net", schemaString()}, {"trans", schemaInt()}})},
            {"zoom",            schemaInt()},
            {"size",            schemaArray(schemaInt())},
            {"layers",          schemaArray(schemaString())},
            {"overlay",         schemaObject({{"nets", schemaBool()}, {"transistors", schemaBool()},
                                              {"latches", schemaBool()}, {"annotations", schemaBool()}})},
            {"highlight_nets",  schemaArray(schemaString())},
            {"highlight_trans", schemaArray(schemaInt())},
            {"highlight_blocks",schemaArray(schemaString(), "Functional block names to outline")},
            {"net_mode",        schemaInt()},
        }, {}),
        [this](const QJsonObject &a, QString &err) { return hndRenderRegion(a, err); }
    });

    registerTool({
        "z80_render_full_die",
        "Render the entire 4700x5000 die to the requested output size, with the chosen layers composited.",
        schemaObject({
            {"size",   schemaArray(schemaInt())},
            {"layers", schemaArray(schemaString())},
        }, {}),
        [this](const QJsonObject &a, QString &err) { return hndRenderFullDie(a, err); }
    });

    registerTool({
        "z80_view_set",
        "Drive the interactive image view(s) to a position and zoom (via ClassController::syncView).",
        schemaObject({
            {"x",    schemaInt()},
            {"y",    schemaInt()},
            {"zoom", schemaInt()},
        }, {}),
        [this](const QJsonObject &a, QString &err) { return hndViewSet(a, err); }
    });

    registerTool({
        "z80_view_grab",
        "Capture the current interactive image view as a PNG (requires a visible DockImageView).",
        schemaObject({}, {}),
        [this](const QJsonObject &a, QString &err) { return hndViewGrab(a, err); }
    });

    // --- Escape hatch --------------------------------------------------------

    registerTool({
        "z80_eval_js",
        "Evaluate an arbitrary JavaScript snippet in the app's QJSEngine. "
        "Use for one-off exploration that does not yet justify a typed tool. "
        "Captured output via ClassScript::print is returned as stdout.",
        schemaObject({{"snippet", schemaString()}}, {"snippet"}),
        [this](const QJsonObject &a, QString &err) { return hndEvalJs(a, err); }
    });

    // --- Topology / waveform helpers -----------------------------------------

    registerTool({
        "z80_fanout",
        "Given a net, return every transistor for which this net is the GATE, "
        "with the transistor's source/drain (c1/c2). Use this to trace a PLA "
        "signal to the transistor gates it controls, then to the nets those "
        "transistors switch.",
        schemaObject({
            {"net", schemaString("Net name or numeric id")},
        }, {"net"}),
        [this](const QJsonObject &a, QString &err) { return hndFanout(a, err); }
    });

    registerTool({
        "z80_watchlist_add",
        "Append the given net names to the waveform watchlist so future "
        "z80_waveform_window calls return full sampled history for them. "
        "Duplicates are ignored. Does NOT clear existing entries.",
        schemaObject({
            {"nets", schemaArray(schemaString(), "Names to append")},
        }, {"nets"}),
        [this](const QJsonObject &a, QString &err) { return hndWatchlistAdd(a, err); }
    });

    registerTool({
        "z80_sample_window",
        "Convenience: add the requested nets to the watchlist, reset the sim, "
        "run N half-cycles, return the sampled table, then restore the prior "
        "watchlist. Use for single-shot captures without touching config.",
        schemaObject({
            {"nets",       schemaArray(schemaString(), "Net names to capture")},
            {"halfcycles", schemaInt("Number of half-cycles to run from reset")},
            {"reset",      schemaBool("Reset before running (default true)")},
        }, {"nets", "halfcycles"}),
        [this](const QJsonObject &a, QString &err) { return hndSampleWindow(a, err); }
    });

    registerTool({
        "z80_equation_tree",
        "Return the logic equation driving a net as a DAG-form JSON tree "
        "instead of the flat string produced by z80_equation. Preserves every "
        "sub-expression (no '...' elisions) and gives each node a stable id so "
        "shared subtrees and cycles collapse to refs. Nodes: "
        "{op, name, net_id, leaf, root, args:[\"nN\", ...]}. Top-level: "
        "{root, nodes, truncated, node_count, id, name}. 'truncated' is true "
        "whenever any DotDot node is present (parser hit its depth budget).",
        schemaObject({
            {"net", schemaString("Net name or numeric id")},
        }, {"net"}),
        [this](const QJsonObject &a, QString &err) { return hndEquationTree(a, err); }
    });

    registerTool({
        "z80_net_drivers",
        "Return the direct transistor-level drivers of a net: for each "
        "transistor where this net appears as source or drain, report its "
        "gate net (the direct driver), the 'other end' net (whichever of "
        "c1/c2 is not this net), and a 'kind' classification: "
        "'pulldown' (other end is GND), 'pullup' (other end is VCC), or "
        "'pass' (other end is another net). No recursion — one hop only. "
        "Use this instead of z80_fanout when you want to answer 'what can "
        "drive this net low/high', not 'what does this net gate'.",
        schemaObject({
            {"net", schemaString("Net name or numeric id")},
        }, {"net"}),
        [this](const QJsonObject &a, QString &err) { return hndNetDrivers(a, err); }
    });
}

// ===========================================================================
// Execution control
// ===========================================================================

QJsonValue ClassMcpTools::hndLoadHex(const QJsonObject &a, QString &err)
{
    const QString path = strArg(a, "path");
    if (path.isEmpty()) { err = "Missing 'path'"; return {}; }
    const bool clear = boolArg(a, "clear", true);

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        bool ok = ::controller.getTrickbox().loadHex(path, clear);
        QJsonObject r; r["ok"] = ok; r["path"] = path; r["clear"] = clear;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndReset(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        uint hc = ::controller.doReset();
        QJsonObject r; r["ok"] = true; r["hc"] = qint64(hc);
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndRun(const QJsonObject &a, QString &)
{
    const int halfcycles = intArg(a, "halfcycles", 0);
    const int timeoutMs  = intArg(a, "timeout_ms", 30000);
    QJsonObject until = a.value("until").toObject();
    const bool wantPc    = until.contains("pc");
    const bool wantCycle = until.contains("cycle");
    const bool wantNet   = until.contains("net");
    const int untilPc    = intArg(until, "pc", -1);
    const int untilCycle = intArg(until, "cycle", -1);
    const QString untilNetName = strArg(until, "net");
    const int untilValue = intArg(until, "value", 0);

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        // Configure breakpoints
        if (wantCycle)
            ::controller.getTrickbox().stopAt(quint16(untilCycle));
        if (wantNet)
        {
            net_t nid = ::controller.getNetlist().get(untilNetName);
            if (nid == 0) { QJsonObject r; r["ok"] = false; r["error"] = "unknown net"; return textResult(r); }
            ::controller.getTrickbox().breakWhen(nid, quint8(untilValue));
        }

        // PC-based stop: poll via a QTimer that checks PC and aborts when matched
        QTimer pcPoll;
        QString stoppedBy;
        if (wantPc)
        {
            pcPoll.setInterval(10);
            QObject::connect(&pcPoll, &QTimer::timeout, [&]() {
                uint16_t pc = ::controller.getSimZ80().getPC();
                if (int(pc) == untilPc)
                {
                    stoppedBy = "pc";
                    ::controller.doRunsim(0);
                    pcPoll.stop();
                }
            });
            pcPoll.start();
        }

        // Start the run: INT_MAX means "run forever until a breakpoint or stop"
        // (ClassController::doRunsim treats this as "Starting simulation")
        uint ticks = halfcycles > 0 ? uint(halfcycles) : uint(INT_MAX);
        const uint hcStart = ::controller.getSimZ80().getCurrentHCycle();
        ::controller.doRunsim(ticks);

        // Wait until it stops (blocking the handler thread via a local event loop)
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&::controller, &ClassController::onRunStopped, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        if (timeoutMs > 0)
            timeoutTimer.start(timeoutMs);
        loop.exec();
        pcPoll.stop();

        if (stoppedBy.isEmpty())
        {
            const uint hcEnd = ::controller.getSimZ80().getCurrentHCycle();
            if (timeoutTimer.isActive()) // Timer hadn't fired; normal stop
                stoppedBy = (halfcycles > 0 && (hcEnd - hcStart) >= uint(halfcycles)) ? "count"
                          : wantCycle ? "cycle"
                          : wantNet   ? "net"
                          : "stop";
            else
                stoppedBy = "timeout";
        }

        // Best-effort clear of cycle/net breakpoints we set
        if (wantCycle)
            ::controller.getTrickbox().stopAt(0);
        if (wantNet)
            ::controller.getTrickbox().breakWhen(0, 0);

        QJsonObject r;
        r["ok"]         = true;
        r["hc_start"]   = qint64(hcStart);
        r["hc_final"]   = qint64(::controller.getSimZ80().getCurrentHCycle());
        r["pc_final"]   = int(::controller.getSimZ80().getPC());
        r["stopped_by"] = stoppedBy;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndStop(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.doRunsim(0);
        QJsonObject r;
        r["ok"] = true;
        r["hc"] = qint64(::controller.getSimZ80().getCurrentHCycle());
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndNow(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QJsonObject r;
        r["hc"]       = qint64(::controller.getSimZ80().getCurrentHCycle());
        r["pc"]       = int(::controller.getSimZ80().getPC());
        r["mt"]       = ::controller.getScript().getMTState();
        r["clk"]      = readBitByName("clk");
        r["running"]  = ::controller.isSimRunning();
        return textResult(r);
    });
}

// ===========================================================================
// State reads
// ===========================================================================

QJsonValue ClassMcpTools::hndNetRead(const QJsonObject &a, QString &err)
{
    QJsonArray nets = a.value("nets").toArray();
    if (nets.isEmpty()) { err = "Empty 'nets'"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QJsonArray out;
        ClassNetlist &nl = ::controller.getNetlist();
        for (const QJsonValue &v : nets)
        {
            QJsonObject e;
            net_t id = resolveNet(v);
            QString name = (id && id < MAX_NETS) ? nl.get(id) : QString();
            pin_t val = id ? readBitByNum(id) : pin_t(3);
            e["id"]    = int(id);
            e["name"]  = name;
            e["value"] = val;
            out.append(e);
        }
        QJsonObject r; r["results"] = out;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndBusRead(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        z80state z;
        ::controller.readState(z);
        QJsonObject r;
        r["AB"]      = z.ab0 == 2 ? QJsonValue(QJsonValue::Null) : QJsonValue(int(z.ab));
        r["DB"]      = z.db0 == 2 ? QJsonValue(QJsonValue::Null) : QJsonValue(int(z.db));
        r["M1"]      = z.m1;
        r["MREQ"]    = z.mreq;
        r["IORQ"]    = z.iorq;
        r["RD"]      = z.rd;
        r["WR"]      = z.wr;
        r["RFSH"]    = z.rfsh;
        r["HALT"]    = z.halt;
        r["BUSAK"]   = z.busak;
        r["INT"]     = z.intr;
        r["NMI"]     = z.nmi;
        r["BUSRQ"]   = z.busrq;
        r["WAIT"]    = z.wait;
        r["RESET"]   = z.reset;
        r["CLK"]     = z.clk;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndRegisterRead(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        z80state z;
        ::controller.readState(z);
        QJsonObject r;
        r["AF"]  = int(z.af);  r["BC"]  = int(z.bc);  r["DE"]  = int(z.de);  r["HL"]  = int(z.hl);
        r["AF2"] = int(z.af2); r["BC2"] = int(z.bc2); r["DE2"] = int(z.de2); r["HL2"] = int(z.hl2);
        r["IX"]  = int(z.ix);  r["IY"]  = int(z.iy);  r["SP"]  = int(z.sp);
        r["IR"]  = int(z.ir);  r["WZ"]  = int(z.wz);  r["PC"]  = int(z.pc);
        r["instr"] = int(z.instr);
        r["nED"]   = z.nED;
        r["nCB"]   = z.nCB;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndTransRead(const QJsonObject &a, QString &err)
{
    QJsonArray ids = a.value("ids").toArray();
    if (ids.isEmpty()) { err = "Empty 'ids'"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QJsonArray out;
        ClassNetlist &nl = ::controller.getNetlist();
        for (const QJsonValue &v : ids)
        {
            const tran_t t = tran_t(v.toInt());
            net_t c1 = 0, c2 = 0;
            nl.getTnet(t, c1, c2);
            QJsonObject e;
            e["id"]     = int(t);
            e["source"] = int(c1);
            e["drain"]  = int(c2);
            e["on"]     = nl.isTransOn(t);
            out.append(e);
        }
        QJsonObject r; r["results"] = out;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndWaveformWindow(const QJsonObject &a, QString &err)
{
    QJsonArray names = a.value("nets").toArray();
    if (names.isEmpty()) { err = "Empty 'nets'"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassWatch &w = ::controller.getWatch();
        const uint curHc = ::controller.getSimZ80().getCurrentHCycle();
        const uint ringStart = w.gethstart();
        int fromHc = intArg(a, "from_hc", int(ringStart));
        int toHc   = intArg(a, "to_hc",   int(curHc));
        if (fromHc < int(ringStart)) fromHc = int(ringStart);
        if (toHc   > int(curHc))     toHc   = int(curHc);
        if (toHc   < fromHc)         toHc   = fromHc;

        QJsonObject samples;
        for (const QJsonValue &nv : names)
        {
            const QString name = nv.toString();
            watch *wp = w.find(name);
            QJsonArray arr;
            if (!wp)
            {
                // Net not in watchlist: return single current value
                arr.append(int(readBitByName(name)));
            }
            else
            {
                for (int hc = fromHc; hc <= toHc; hc++)
                    arr.append(int(w.at(wp, uint(hc))));
            }
            samples[name] = arr;
        }

        QJsonObject r;
        r["from_hc"] = fromHc;
        r["to_hc"]   = toHc;
        r["samples"] = samples;
        return textResult(r);
    });
}

// ===========================================================================
// Metadata / discovery
// ===========================================================================

QJsonValue ClassMcpTools::hndNetFind(const QJsonObject &a, QString &err)
{
    const QString pattern = strArg(a, "pattern");
    if (pattern.isEmpty()) { err = "Missing 'pattern'"; return {}; }
    QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
    if (!re.isValid()) { err = QString("Invalid pattern: %1").arg(re.errorString()); return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassNetlist &nl = ::controller.getNetlist();
        QStringList all = nl.getNetnames();
        QJsonArray hits;
        for (const QString &n : all)
        {
            if (re.match(n).hasMatch())
            {
                QJsonObject e;
                e["name"] = n;
                e["id"]   = int(nl.get(n));
                hits.append(e);
                if (hits.size() >= 500) break; // cap
            }
        }
        QJsonObject r; r["matches"] = hits; r["total"] = hits.size();
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndNetInfo(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassNetlist &nl = ::controller.getNetlist();
        net_t id = resolveNet(a.value("net"));
        if (id == 0) { QJsonObject r; r["error"] = "unknown net"; return textResult(r); }
        QJsonObject r;
        r["id"]         = int(id);
        r["name"]       = nl.get(id);
        r["info"]       = nl.netInfo(id);
        r["has_pullup"] = nl.isNetPulledUp(id);
        r["is_orphan"]  = nl.isNetOrphan(id);
        r["is_gateless"]= nl.isNetGateless(id);
        r["state"]      = int(readBitByNum(id));

        QJsonArray drivers, driven;
        for (net_t n : nl.netsDriving(id))  drivers.append(int(n));
        for (net_t n : nl.netsDriven(id))   driven.append(int(n));
        r["nets_driving"] = drivers;
        r["nets_driven"]  = driven;

        if (m_spatial)
        {
            QRect b = m_spatial->netBBox(id);
            if (!b.isEmpty())
            {
                QJsonArray bb;
                bb.append(b.left()); bb.append(b.top()); bb.append(b.width()); bb.append(b.height());
                r["bbox"] = bb;
            }
        }
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndTransInfo(const QJsonObject &a, QString &err)
{
    const int id = intArg(a, "id", -1);
    if (id < 0) { err = "Missing 'id'"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassNetlist &nl = ::controller.getNetlist();
        ClassVisual  &cv = ::controller.getChip();
        QJsonObject r;
        r["id"] = id;
        net_t c1 = 0, c2 = 0;
        nl.getTnet(tran_t(id), c1, c2);
        const transvdef *tv = cv.getTrans(tran_t(id));
        r["gate_net"]   = tv ? int(tv->gatenet) : 0;
        r["source_net"] = int(c1);
        r["drain_net"]  = int(c2);
        r["on"]         = nl.isTransOn(tran_t(id));
        r["info"]       = nl.transInfo(tran_t(id));
        if (tv)
        {
            QJsonArray box;
            box.append(tv->box.left()); box.append(tv->box.top());
            box.append(tv->box.width()); box.append(tv->box.height());
            r["box"] = box;
        }
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndEquation(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        net_t id = resolveNet(a.value("net"));
        if (id == 0) { QJsonObject r; r["error"] = "unknown net"; return textResult(r); }
        QJsonObject r;
        r["id"]   = int(id);
        r["name"] = ::controller.getNetlist().get(id);
        r["expr"] = ::controller.getNetlist().equation(id);
        return textResult(r);
    });
}

// ===========================================================================
// Memory / IO
// ===========================================================================

QJsonValue ClassMcpTools::hndMemRead(const QJsonObject &a, QString &err)
{
    const int addr = intArg(a, "addr", -1);
    const int len  = intArg(a, "len", -1);
    if (addr < 0 || addr > 0xFFFF) { err = "addr out of range [0..0xFFFF]"; return {}; }
    if (len  < 0 || len  > 0x10000) { err = "len out of range"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QJsonArray bytes;
        for (int i = 0; i < len; i++)
        {
            const int a2 = (addr + i) & 0xFFFF;
            bytes.append(int(::controller.readMem(quint16(a2))));
        }
        QJsonObject r; r["addr"] = addr; r["len"] = len; r["bytes"] = bytes;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndMemWrite(const QJsonObject &a, QString &err)
{
    const int addr = intArg(a, "addr", -1);
    QJsonArray bytes = a.value("bytes").toArray();
    if (addr < 0 || addr > 0xFFFF) { err = "addr out of range"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        int i = 0;
        for (const QJsonValue &v : bytes)
        {
            const int a2 = (addr + i) & 0xFFFF;
            ::controller.writeMem(quint16(a2), quint8(v.toInt() & 0xFF));
            i++;
        }
        QJsonObject r; r["ok"] = true; r["written"] = i;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndIoRead(const QJsonObject &a, QString &err)
{
    const int addr = intArg(a, "addr", -1);
    if (addr < 0 || addr > 0xFFFF) { err = "addr out of range"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QJsonObject r;
        r["addr"] = addr;
        r["byte"] = int(::controller.readIO(quint16(addr)));
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndIoWrite(const QJsonObject &a, QString &err)
{
    const int addr = intArg(a, "addr", -1);
    const int byte = intArg(a, "byte", -1);
    if (addr < 0 || addr > 0xFFFF || byte < 0 || byte > 0xFF) { err = "out of range"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.writeIO(quint16(addr), quint8(byte));
        QJsonObject r; r["ok"] = true;
        return textResult(r);
    });
}

// ===========================================================================
// Pin control
// ===========================================================================

QJsonValue ClassMcpTools::hndPinSet(const QJsonObject &a, QString &err)
{
    const QString pin = strArg(a, "pin");
    const int value = intArg(a, "value", -1);
    if (pin.isEmpty() || value < 0) { err = "Missing pin or value"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.getTrickbox().set(pin, quint8(value));
        QJsonObject r; r["ok"] = true; r["pin"] = pin; r["value"] = value;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndPinSetAt(const QJsonObject &a, QString &err)
{
    const QString pin = strArg(a, "pin");
    const int at_hc = intArg(a, "at_hc", -1);
    const int hold  = intArg(a, "hold", -1);
    if (pin.isEmpty() || at_hc < 0 || hold < 0) { err = "Missing pin/at_hc/hold"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.getTrickbox().setAt(pin, quint16(at_hc), quint16(hold));
        QJsonObject r; r["ok"] = true;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndPinSetAtPc(const QJsonObject &a, QString &err)
{
    const QString pin = strArg(a, "pin");
    const int pc   = intArg(a, "pc", -1);
    const int hold = intArg(a, "hold", -1);
    if (pin.isEmpty() || pc < 0 || hold < 0) { err = "Missing pin/pc/hold"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.getTrickbox().setAtPC(pin, quint16(pc), quint16(hold));
        QJsonObject r; r["ok"] = true;
        return textResult(r);
    });
}

// ===========================================================================
// Breakpoints
// ===========================================================================

QJsonValue ClassMcpTools::hndBreakAdd(const QJsonObject &a, QString &err)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ActiveBreakpoint bp;
        bp.id = m_nextBreakpointId++;
        if (a.contains("pc"))
        {
            bp.kind = "pc";
            bp.arg1 = intArg(a, "pc", 0);
            // No dedicated "stop at pc" engine; keep the record, hndRun polls PC on demand.
        }
        else if (a.contains("cycle"))
        {
            bp.kind = "cycle";
            bp.arg1 = intArg(a, "cycle", 0);
            ::controller.getTrickbox().stopAt(quint16(bp.arg1));
        }
        else if (a.contains("net"))
        {
            bp.kind = "net";
            net_t nid = ::controller.getNetlist().get(strArg(a, "net"));
            bp.arg1 = int(nid);
            bp.arg2 = intArg(a, "value", 0);
            if (nid == 0) { err = "unknown net"; return {}; }
            ::controller.getTrickbox().breakWhen(nid, quint8(bp.arg2));
        }
        else
        {
            err = "break_add: provide exactly one of pc/cycle/net";
            return {};
        }
        m_breakpoints.append(bp);
        QJsonObject r; r["id"] = bp.id; r["kind"] = bp.kind;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndBreakClear(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        const bool all = !a.contains("id");
        const int id = intArg(a, "id", -1);
        QJsonArray cleared;
        QVector<ActiveBreakpoint> keep;
        for (const ActiveBreakpoint &bp : m_breakpoints)
        {
            if (all || bp.id == id)
            {
                cleared.append(bp.id);
                if (bp.kind == "cycle") ::controller.getTrickbox().stopAt(0);
                if (bp.kind == "net")   ::controller.getTrickbox().breakWhen(0, 0);
            }
            else
                keep.append(bp);
        }
        m_breakpoints = keep;
        QJsonObject r; r["cleared"] = cleared;
        return textResult(r);
    });
}

// ===========================================================================
// Spatial queries
// ===========================================================================

static QJsonArray rectToJson(const QRect &r)
{
    QJsonArray a;
    a.append(r.left()); a.append(r.top()); a.append(r.width()); a.append(r.height());
    return a;
}

QJsonValue ClassMcpTools::hndRegionOf(const QJsonObject &a, QString &err)
{
    if (!m_spatial) { err = "Spatial index not available"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        RegionInfo info;
        if (a.contains("net"))
        {
            net_t n = resolveNet(a.value("net"));
            if (n == 0) { err = "unknown net"; return QJsonValue{}; }
            info = m_spatial->regionOfNet(n);
        }
        else if (a.contains("trans"))
        {
            tran_t t = tran_t(intArg(a, "trans", 0));
            info = m_spatial->regionOfTrans(t);
        }
        else
        {
            err = "Provide 'net' or 'trans'";
            return QJsonValue{};
        }
        QJsonObject r;
        r["block"] = info.blockName;
        r["bbox"]  = rectToJson(info.bbox);
        QJsonArray ann;
        for (const QString &s : info.nearbyAnnotations) ann.append(s);
        r["nearby_annotations"] = ann;
        r["valid"] = info.valid;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndNetsNear(const QJsonObject &a, QString &err)
{
    if (!m_spatial) { err = "Spatial index not available"; return {}; }
    const qreal x = intArg(a, "x", -1);
    const qreal y = intArg(a, "y", -1);
    const qreal r = intArg(a, "radius", 0);
    if (x < 0 || y < 0 || r <= 0) { err = "x/y/radius required and positive"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassNetlist &nl = ::controller.getNetlist();
        auto hits = m_spatial->netsNear(x, y, r);
        QJsonArray out;
        for (const SpatialHit &h : hits)
        {
            QJsonObject e;
            e["id"]   = int(h.id);
            e["name"] = nl.get(net_t(h.id));
            e["dist"] = h.dist;
            e["bbox"] = rectToJson(h.bbox);
            out.append(e);
            if (out.size() >= 200) break;
        }
        QJsonObject ro; ro["hits"] = out; ro["total"] = hits.size();
        return textResult(ro);
    });
}

QJsonValue ClassMcpTools::hndTransNear(const QJsonObject &a, QString &err)
{
    if (!m_spatial) { err = "Spatial index not available"; return {}; }
    const qreal x = intArg(a, "x", -1);
    const qreal y = intArg(a, "y", -1);
    const qreal r = intArg(a, "radius", 0);
    if (x < 0 || y < 0 || r <= 0) { err = "x/y/radius required and positive"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        auto hits = m_spatial->transNear(x, y, r);
        QJsonArray out;
        for (const SpatialHit &h : hits)
        {
            QJsonObject e;
            e["id"]   = int(h.id);
            e["dist"] = h.dist;
            e["box"]  = rectToJson(h.bbox);
            out.append(e);
            if (out.size() >= 200) break;
        }
        QJsonObject ro; ro["hits"] = out; ro["total"] = hits.size();
        return textResult(ro);
    });
}

QJsonValue ClassMcpTools::hndBoundingBox(const QJsonObject &a, QString &err)
{
    if (!m_spatial) { err = "Spatial index not available"; return {}; }
    const QString type = strArg(a, "type");
    const QJsonArray ids = a.value("ids").toArray();
    if (type != "net" && type != "trans") { err = "type must be 'net' or 'trans'"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        BBoxInfo info;
        if (type == "net")
        {
            QVector<net_t> ns;
            for (const QJsonValue &v : ids) ns.append(net_t(v.toInt()));
            info = m_spatial->boundingBoxOfNets(ns);
        }
        else
        {
            QVector<tran_t> ts;
            for (const QJsonValue &v : ids) ts.append(tran_t(v.toInt()));
            info = m_spatial->boundingBoxOfTrans(ts);
        }
        QJsonObject r;
        r["valid"]       = info.valid;
        r["bbox"]        = rectToJson(info.bbox);
        r["compactness"] = info.compactness;
        return textResult(r);
    });
}

// ===========================================================================
// Visual / rendering
// ===========================================================================

QJsonValue ClassMcpTools::hndRenderRegion(const QJsonObject &a, QString &err)
{
    if (!m_renderer) { err = "Renderer not available"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        RenderSpec spec;

        // Output size
        QJsonArray size = a.value("size").toArray();
        if (size.size() == 2)
            spec.outputSize = QSize(size[0].toInt(), size[1].toInt());
        spec.outputSize = spec.outputSize.boundedTo(QSize(4096, 4096));
        if (spec.outputSize.width() < 64 || spec.outputSize.height() < 64)
            spec.outputSize = QSize(qMax(64, spec.outputSize.width()), qMax(64, spec.outputSize.height()));

        // Layers
        QJsonArray layers = a.value("layers").toArray();
        for (const QJsonValue &v : layers) spec.layerNames.append(v.toString());

        // Overlays
        QJsonObject overlay = a.value("overlay").toObject();
        spec.drawNets         = overlay.value("nets").toBool(true);
        spec.drawTransistors  = overlay.value("transistors").toBool(false);
        spec.drawLatches      = overlay.value("latches").toBool(false);
        spec.drawAnnotations  = overlay.value("annotations").toBool(true);
        spec.netMode          = uintArg(a, "net_mode", 0);

        // Center + zoom → worldRect
        QJsonObject center = a.value("center").toObject();
        qreal cx = 2350, cy = 2500; // die centre default
        if (center.contains("x") && center.contains("y"))
        {
            cx = center.value("x").toDouble();
            cy = center.value("y").toDouble();
        }
        else if (center.contains("net"))
        {
            net_t n = resolveNet(center.value("net"));
            QRect b = m_spatial ? m_spatial->netBBox(n) : QRect();
            if (!b.isEmpty()) { cx = b.center().x(); cy = b.center().y(); }
        }
        else if (center.contains("trans"))
        {
            tran_t t = tran_t(center.value("trans").toInt());
            QRect b = m_spatial ? m_spatial->transBox(t) : QRect();
            if (!b.isEmpty()) { cx = b.center().x(); cy = b.center().y(); }
        }
        qreal zoom = a.value("zoom").toDouble(1.0);
        if (zoom <= 0) zoom = 1.0;
        // At zoom 1.0 show 1000x1000 of the die; at zoom 2 show 500x500, etc.
        qreal span = 1000.0 / zoom;
        spec.worldRect = QRectF(cx - span * 0.5, cy - span * 0.5, span, span);

        // Highlights
        QJsonArray hn = a.value("highlight_nets").toArray();
        for (const QJsonValue &v : hn) spec.highlightNets.append(resolveNet(v));
        QJsonArray ht = a.value("highlight_trans").toArray();
        for (const QJsonValue &v : ht) spec.highlightTrans.append(tran_t(v.toInt()));
        QJsonArray hb = a.value("highlight_blocks").toArray();
        if (m_spatial && !hb.isEmpty())
        {
            QSet<QString> wanted;
            for (const QJsonValue &v : hb) wanted.insert(v.toString());
            for (const FunctionalBlock &b : m_spatial->blocks())
                if (wanted.contains(b.name))
                    spec.highlightRects.append(b.rect);
        }

        RenderResult result = m_renderer->renderRegion(spec);
        QByteArray png = ClassRenderer::encodePng(result.image);

        QJsonObject meta;
        meta["width"]  = result.image.width();
        meta["height"] = result.image.height();
        meta["world"]  = rectToJson(result.worldRect.toAlignedRect());
        meta["zoom"]   = zoom;
        return mixedResult(png, meta);
    });
}

QJsonValue ClassMcpTools::hndRenderFullDie(const QJsonObject &a, QString &err)
{
    if (!m_renderer) { err = "Renderer not available"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QJsonArray size = a.value("size").toArray();
        QSize out(1024, 1024);
        if (size.size() == 2) out = QSize(size[0].toInt(), size[1].toInt());
        out = out.boundedTo(QSize(4096, 4096));
        QStringList layers;
        for (const QJsonValue &v : a.value("layers").toArray()) layers.append(v.toString());

        RenderResult r = m_renderer->renderFullDie(out, layers);
        QByteArray png = ClassRenderer::encodePng(r.image);
        QJsonObject meta;
        meta["width"]  = r.image.width();
        meta["height"] = r.image.height();
        meta["world"]  = rectToJson(r.worldRect.toAlignedRect());
        return mixedResult(png, meta);
    });
}

QJsonValue ClassMcpTools::hndViewSet(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        const qreal x = a.value("x").toDouble(-1);
        const qreal y = a.value("y").toDouble(-1);
        const qreal z = a.value("zoom").toDouble(-1);
        if (x >= 0 && y >= 0)
        {
            // Normalize to 0..1 "texture" coordinates as the views expect
            const qreal tx = x / 4700.0;
            const qreal ty = y / 5000.0;
            emit ::controller.syncView(QPointF(tx, ty), z > 0 ? z : 1.0);
        }
        QJsonObject r; r["ok"] = true;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndViewGrab(const QJsonObject &, QString &err)
{
    // Grabbing the interactive view requires a visible DockImageView; not all
    // sessions have one mapped. Prefer render_region / render_full_die which
    // always work. We attempt a grab of the first top-level widget whose
    // object name begins with "DockImageView" if one exists.
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QWidgetList widgets = QApplication::topLevelWidgets();
        QWidget *found = nullptr;
        for (QWidget *w : widgets)
        {
            QWidget *child = w->findChild<QWidget*>("widgetImageView");
            if (child) { found = child; break; }
        }
        if (!found) { err = "No DockImageView found; use render_region instead"; return QJsonValue{}; }
        QPixmap p = found->grab();
        QImage img = p.toImage();
        QByteArray png = ClassRenderer::encodePng(img);
        return imageResult(png, "Interactive view grab");
    });
}

// ===========================================================================
// Escape hatch
// ===========================================================================

QJsonValue ClassMcpTools::hndEvalJs(const QJsonObject &a, QString &err)
{
    const QString snippet = strArg(a, "snippet");
    if (snippet.isEmpty()) { err = "Missing 'snippet'"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassScript &sc = ::controller.getScript();
        QStringList captured;
        auto conn = QObject::connect(&sc, &ClassScript::print, &sc,
                                     [&captured](QString msg) { captured.append(msg); });
        sc.exec(snippet, /*echo*/ false);
        QObject::disconnect(conn);
        QJsonObject r;
        r["ok"]     = true;
        r["stdout"] = captured.join("\n");
        return textResult(r);
    });
}

// ===========================================================================
// Topology + waveform helpers
// ===========================================================================

QJsonValue ClassMcpTools::hndFanout(const QJsonObject &a, QString &err)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassNetlist &nl = ::controller.getNetlist();
        net_t id = resolveNet(a.value("net"));
        if (id == 0) { err = "unknown net"; return QJsonValue{}; }

        QVector<tran_t> gated = nl.getGatedTransistors(id);
        QVector<tran_t> connected = nl.getConnectedTransistors(id);

        QJsonArray gArr;
        for (tran_t t : gated)
        {
            net_t c1 = 0, c2 = 0;
            nl.getTnet(t, c1, c2);
            QJsonObject e;
            e["id"]     = int(t);
            e["c1"]     = int(c1);
            e["c2"]     = int(c2);
            e["on"]     = nl.isTransOn(t);
            gArr.append(e);
        }

        QJsonArray cArr;
        for (tran_t t : connected)
        {
            net_t c1 = 0, c2 = 0;
            nl.getTnet(t, c1, c2);
            QJsonObject e;
            e["id"]     = int(t);
            e["gate"]   = int(nl.getTransGate(t));
            e["c1"]     = int(c1);
            e["c2"]     = int(c2);
            e["on"]     = nl.isTransOn(t);
            cArr.append(e);
        }

        QJsonObject r;
        r["id"]        = int(id);
        r["name"]      = nl.get(id);
        r["gates"]     = gArr;           // transistors this net controls (it is their gate)
        r["connects"]  = cArr;           // transistors where this net is source or drain
        r["gate_count"]      = gArr.size();
        r["connect_count"]   = cArr.size();
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndWatchlistAdd(const QJsonObject &a, QString &err)
{
    QJsonArray names = a.value("nets").toArray();
    if (names.isEmpty()) { err = "Empty 'nets'"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassWatch &w = ::controller.getWatch();
        QStringList current = w.getWatchlist();
        QStringList added;
        QStringList skipped;
        for (const QJsonValue &v : names)
        {
            const QString n = v.toString();
            if (n.isEmpty()) continue;
            if (current.contains(n)) { skipped.append(n); continue; }
            // The watch stores the name and resolves it at sample time, so we
            // accept any non-empty string. Callers can grep the returned list
            // afterwards to confirm it stuck.
            current.append(n);
            added.append(n);
        }
        w.updateWatchlist(current);
        QJsonObject r;
        r["added"]    = QJsonArray::fromStringList(added);
        r["skipped"]  = QJsonArray::fromStringList(skipped);
        r["total"]    = int(w.getWatchlistLen());
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndSampleWindow(const QJsonObject &a, QString &err)
{
    QJsonArray names = a.value("nets").toArray();
    const int halfcycles = intArg(a, "halfcycles", 0);
    const bool doReset   = boolArg(a, "reset", true);
    if (names.isEmpty() || halfcycles <= 0) { err = "Missing 'nets' or 'halfcycles'"; return {}; }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassWatch &w = ::controller.getWatch();
        // Save prior watchlist
        QStringList prior = w.getWatchlist();
        QStringList merged = prior;
        for (const QJsonValue &v : names)
        {
            const QString n = v.toString();
            if (!n.isEmpty() && !merged.contains(n))
                merged.append(n);
        }
        w.updateWatchlist(merged);

        uint hcStart = 0;
        if (doReset)
        {
            ::controller.doReset();
            hcStart = 0;
        }
        else
        {
            hcStart = ::controller.getSimZ80().getCurrentHCycle();
        }

        // Run the window and wait for completion
        ::controller.doRunsim(uint(halfcycles));
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&::controller, &ClassController::onRunStopped, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeoutTimer.start(30000);
        loop.exec();

        uint hcEnd = ::controller.getSimZ80().getCurrentHCycle();
        uint ringStart = w.gethstart();
        uint fromHc = qMax(hcStart, ringStart);
        uint toHc   = hcEnd;

        QJsonObject samples;
        for (const QJsonValue &nv : names)
        {
            const QString name = nv.toString();
            if (name.isEmpty()) continue;
            watch *wp = w.find(name);
            QJsonArray arr;
            if (!wp)
            {
                arr.append(int(readBitByName(name)));
            }
            else
            {
                for (uint hc = fromHc; hc <= toHc; hc++)
                    arr.append(int(w.at(wp, hc)));
            }
            samples[name] = arr;
        }

        // Restore prior watchlist
        w.updateWatchlist(prior);

        QJsonObject r;
        r["hc_start"] = qint64(fromHc);
        r["hc_end"]   = qint64(toHc);
        r["samples"]  = samples;
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndEquationTree(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        net_t id = resolveNet(a.value("net"));
        if (id == 0) { QJsonObject r; r["error"] = "unknown net"; return textResult(r); }
        QJsonObject r = ::controller.getNetlist().equationTreeJson(id);
        return textResult(r);
    });
}

QJsonValue ClassMcpTools::hndNetDrivers(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        net_t id = resolveNet(a.value("net"));
        if (id == 0) { QJsonObject r; r["error"] = "unknown net"; return textResult(r); }
        QJsonObject r = ::controller.getNetlist().netDriversJson(id);
        return textResult(r);
    });
}
