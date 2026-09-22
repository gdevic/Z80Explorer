#include "ClassMcpTools.h"
#include "ClassAnnotate.h"
#include "ClassController.h"
#include "ClassMcpThreading.h"
#include "ClassNetlist.h"
#include "ClassScript.h"
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
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QTimer>
#include <QWidget>
#include <climits>

ClassMcpTools::ClassMcpTools(QObject *parent)
    : QObject(parent)
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

/*
 * The payload travels twice: once as structuredContent, which a client validates against the tool's
 * outputSchema and hands to the model as data, and once serialized into a text block for clients
 * that only render text. The spec asks for both, and the second costs one serialization.
 */
QJsonValue ClassMcpTools::structuredResult(const QJsonValue &jsonPayload)
{
    QJsonDocument doc(jsonPayload.isObject() ? jsonPayload.toObject() : QJsonObject{{"value", jsonPayload}});
    QJsonObject item; item["type"] = "text"; item["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    QJsonObject r;
    r["content"]           = QJsonArray{ item };
    r["structuredContent"] = jsonPayload;
    r["isError"]           = false;
    return r;
}

QJsonValue ClassMcpTools::errorResult(const QString &message)
{
    return textResult(message, true);
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

/*
 * Names are how a model addresses a net, and it will get them wrong. Returning the nearest existing
 * names turns a dead end into a correctable one: the model retries with a real name instead of
 * guessing again or giving up.
 */
QStringList ClassMcpTools::nearestNetNames(const QString &name, int limit) const
{
    QStringList exact, prefix, contains;
    const QString needle = name.toLower();
    ClassNetlist &nl = ::controller.getNetlist();
    for (net_t i = 1; i < MAX_NETS; i++)
    {
        const QString &n = nl.get(i);
        if (n.isEmpty())
            continue;
        const QString ln = n.toLower();
        if (ln == needle)
            exact.append(n);
        else if (ln.startsWith(needle) || needle.startsWith(ln))
            prefix.append(n);
        else if (ln.contains(needle) || needle.contains(ln))
            contains.append(n);
        if (exact.size() >= limit)
            break;
    }
    QStringList out = exact + prefix + contains;
    if (out.size() > limit)
        out = out.mid(0, limit);
    return out;
}

/*
 * Range-checked net resolution. Out-of-range ids used to be read straight out of the netlist arrays,
 * which is a buffer overread in a release build where the asserts are gone.
 */
net_t ClassMcpTools::resolveNetChecked(const QJsonValue &v, QString &err) const
{
    if (v.isUndefined() || v.isNull())
    {
        err = QStringLiteral("Missing net argument. Pass a net name or a numeric id.");
        return 0;
    }

    if (v.isDouble())
    {
        const int n = v.toInt(-1);
        if ((n <= 0) || (n >= int(MAX_NETS)))
        {
            err = QString("Net id %1 is out of range; valid ids are 1..%2.").arg(n).arg(MAX_NETS - 1);
            return 0;
        }
        return net_t(n);
    }

    if (v.isString())
    {
        const QString s = v.toString();
        bool ok = false;
        const int n = s.toInt(&ok, 0);
        if (ok)
        {
            if ((n <= 0) || (n >= int(MAX_NETS)))
            {
                err = QString("Net id %1 is out of range; valid ids are 1..%2.").arg(n).arg(MAX_NETS - 1);
                return 0;
            }
            return net_t(n);
        }
        const net_t id = ::controller.getNetlist().get(s);
        if (id == 0)
        {
            const QStringList near = nearestNetNames(s);
            err = near.isEmpty()
                ? QString("Unknown net \"%1\". Use z80_net_find to search net names.").arg(s)
                : QString("Unknown net \"%1\". Did you mean: %2? Use z80_net_find to search.")
                      .arg(s, near.join(QStringLiteral(", ")));
            return 0;
        }
        return id;
    }

    err = QStringLiteral("A net must be given as a string name or an integer id.");
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
        if (!t.title.isEmpty())
            o["title"] = t.title;
        if (!t.outputSchema.isEmpty())
            o["outputSchema"] = t.outputSchema;

        QJsonObject ann;
        ann["readOnlyHint"]    = t.hints.readOnly;
        ann["destructiveHint"] = t.hints.destructive;
        ann["idempotentHint"]  = t.hints.idempotent;
        ann["openWorldHint"]   = t.hints.openWorld;
        if (!t.title.isEmpty())
            ann["title"] = t.title;
        o["annotations"] = ann;

        out.append(o);
    }
    return out;
}

bool ClassMcpTools::hasTool(const QString &name) const
{
    for (const ToolDef &t : m_tools)
        if (t.name == name)
            return true;
    return false;
}

bool ClassMcpTools::isReentrant(const QString &name) const
{
    for (const ToolDef &t : m_tools)
        if (t.name == name)
            return t.reentrant;
    return true;
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
// A tool that takes no arguments. Spelling it this way says "accepts only an empty object" rather
// than "accepts any object", which is what an empty properties list actually means.
static QJsonObject schemaNoArgs()
{
    QJsonObject s; s["type"] = "object"; s["additionalProperties"] = false;
    return s;
}
// A net may be addressed by name or by number anywhere one is taken, so say so in the schema
// instead of leaving a client to infer it from the description.
static QJsonObject schemaNetRef(const QString &desc = {})
{
    QJsonObject s;
    s["anyOf"] = QJsonArray{
        QJsonObject{{"type", "string"}, {"description", "Net name"}},
        QJsonObject{{"type", "integer"}, {"minimum", 1}, {"description", "Net number"}},
    };
    s["description"] = desc.isEmpty() ? QStringLiteral("Net name or numeric id") : desc;
    return s;
}
// The pins the trickbox can drive. An enum turns a misspelling into a client-side rejection rather
// than a call that reports success and does nothing.
static QJsonObject schemaPin()
{
    QJsonObject s;
    s["type"] = "string";
    s["enum"] = QJsonArray{ "int", "nmi", "busrq", "wait", "reset" };
    s["description"] = QStringLiteral("Pin to drive. Case-insensitive.");
    return s;
}

// ===========================================================================
// Register the built-in tools
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
        schemaNoArgs(),
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
                              {"net",   schemaNetRef()},
                              {"value", schemaInt()},
                          })},
            {"timeout_ms", schemaInt("Wall-clock timeout in milliseconds; default 30000")},
        }, {}),
        [this](const QJsonObject &a, QString &err) { return hndRun(a, err); }
    });

    registerTool({
        "z80_stop",
        "Stop the running simulation immediately.",
        schemaNoArgs(),
        [this](const QJsonObject &a, QString &err) { return hndStop(a, err); }
    });

    registerTool({
        "z80_now",
        "Return the current sim status: half-cycle, PC, MT state, clk level, running flag.",
        schemaNoArgs(),
        [this](const QJsonObject &a, QString &err) { return hndNow(a, err); }
    });

    // --- State reads ---------------------------------------------------------

    registerTool({
        "z80_net_read",
        "Batch-read the current logic value of the listed nets. Each entry may be a net name or numeric id. Returns {id, name, value} where value is 0, 1, or 2 (hi-Z).",
        schemaObject({
            {"nets", schemaArray(schemaNetRef(), "Nets to read, each a name or a numeric id")},
        }, {"nets"}),
        [this](const QJsonObject &a, QString &err) { return hndNetRead(a, err); }
    });

    registerTool({
        "z80_bus_read",
        "Read the external bus pins: AB, DB, /M1, /MREQ, /IORQ, /RD, /WR, /RFSH, /HALT, /BUSAK.",
        schemaNoArgs(),
        [this](const QJsonObject &a, QString &err) { return hndBusRead(a, err); }
    });

    registerTool({
        "z80_register_read",
        "Read the Z80 CPU registers (PC, IR, WZ, AF, HL, DE, BC, IX, IY, SP, I, R) and alternates.",
        schemaNoArgs(),
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
            {"nets",    schemaArray(schemaNetRef(), "Nets to watch, each a name or a numeric id")},
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
            {"net", schemaNetRef()},
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
            {"net", schemaNetRef()},
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
            {"pin",   schemaPin()},
            {"value", schemaInt("0 asserted (active-low), 1 de-asserted, 2 hi-Z if supported")},
        }, {"pin", "value"}),
        [this](const QJsonObject &a, QString &err) { return hndPinSet(a, err); }
    });

    registerTool({
        "z80_pin_set_at",
        "Schedule a pin assertion at a specific half-cycle, held for N half-cycles.",
        schemaObject({
            {"pin",   schemaPin()},
            {"at_hc", schemaInt()},
            {"hold",  schemaInt()},
        }, {"pin", "at_hc", "hold"}),
        [this](const QJsonObject &a, QString &err) { return hndPinSetAt(a, err); }
    });

    registerTool({
        "z80_pin_set_at_pc",
        "Schedule a pin assertion when the PC reaches a given address, held for N half-cycles.",
        schemaObject({
            {"pin",  schemaPin()},
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
            {"net",   schemaNetRef()},
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

    // --- Interactive image view ---------------------------------------------

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
        "Trigger the host-side 'Export PNG' file save dialog on a visible image view "
        "(WidgetImageView::onPng). The user picks the destination file. Requires a "
        "visible DockImageView. Returns ok status; no inline image.",
        schemaNoArgs(),
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
            {"net", schemaNetRef()},
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
            {"net", schemaNetRef()},
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
            {"net", schemaNetRef()},
        }, {"net"}),
        [this](const QJsonObject &a, QString &err) { return hndNetDrivers(a, err); }
    });

    registerTool({
        "z80_rename_net",
        "Rename a net that already has a name. The existing name is removed "
        "and replaced with the new one. Errors if the net has no existing "
        "name (use z80_eval_js setNetName(...) for first-time naming) or if "
        "the new name is already taken by another net. Persists on app "
        "shutdown or via z80_eval_js saveNetnames().",
        schemaObject({
            {"net",  schemaNetRef("Existing net name or numeric id")},
            {"name", schemaString("New name to assign")},
        }, {"net", "name"}),
        [this](const QJsonObject &a, QString &err) { return hndRenameNet(a, err); }
    });

    registerTool({
        "z80_delete_net_name",
        "Clear the name of a named net. Errors if the net has no name. "
        "Persists on app shutdown or via z80_eval_js saveNetnames().",
        schemaObject({
            {"net", schemaNetRef()},
        }, {"net"}),
        [this](const QJsonObject &a, QString &err) { return hndDeleteNetName(a, err); }
    });

    applyToolMetadata();
}

// ===========================================================================
// Tool metadata
// ===========================================================================

// Shape of one net reading, shared by the tools that report net values
static QJsonObject schemaNetValue()
{
    return schemaObject({
        {"id",    schemaInt("Net number, 0 when the net could not be resolved")},
        {"name",  schemaString("Net name, empty when the net is unnamed")},
        {"value", schemaInt("0 low, 1 high, 2 floating. Absent when the entry carries an error")},
        {"error", schemaString("Why this entry could not be read; absent on success")},
    }, {"id"});
}

/*
 * One table describing every tool's behaviour. The hints are what let a host separate the harmless
 * reads from the calls that move the simulation or reach outside it, so a user can approve a class
 * of tools instead of each call. `reentrant` is a local concern rather than an MCP field: a long
 * tool spins a nested event loop, and anything that would disturb the run in progress must be
 * refused while it is held rather than executed underneath it.
 */
void ClassMcpTools::applyToolMetadata()
{
    struct MetaRow
    {
        const char *name;
        const char *title;
        ToolHints hints;
        bool reentrant;
    };

    //                                               readOnly destructive idempotent openWorld
    const ToolHints kRead   { true,  false, true,  false };  // pure observation
    const ToolHints kWrite  { false, false, true,  false };  // changes state, repeatable
    const ToolHints kStep   { false, false, false, false };  // advances time; never idempotent
    const ToolHints kWipe   { false, true,  false, false };  // discards state the user may want
    const ToolHints kHost   { false, true,  false, true  };  // reaches outside the simulator

    const MetaRow kMeta[] = {
        { "z80_load_hex",       "Load HEX program",        kHost,  false },
        { "z80_reset",          "Reset chip",              kWipe,  false },
        { "z80_run",            "Run simulation",          kStep,  false },
        { "z80_stop",           "Stop simulation",         kWrite, true  },
        { "z80_now",            "Current sim status",      kRead,  true  },

        { "z80_net_read",       "Read nets",               kRead,  true  },
        { "z80_bus_read",       "Read buses",              kRead,  true  },
        { "z80_register_read",  "Read registers",          kRead,  true  },
        { "z80_trans_read",     "Read transistor",         kRead,  true  },
        { "z80_waveform_window","Read waveform history",   kRead,  true  },

        { "z80_net_find",       "Search net names",        kRead,  true  },
        { "z80_net_info",       "Net details",             kRead,  true  },
        { "z80_trans_info",     "Transistor details",      kRead,  true  },
        { "z80_equation",       "Net logic equation",      kRead,  true  },
        { "z80_equation_tree",  "Net logic tree",          kRead,  true  },
        { "z80_net_drivers",    "Net drivers",             kRead,  true  },
        { "z80_fanout",         "Net fanout",              kRead,  true  },

        { "z80_mem_read",       "Read memory",             kRead,  true  },
        { "z80_mem_write",      "Write memory",            kWrite, false },
        { "z80_io_read",        "Read I/O port",           kRead,  true  },
        { "z80_io_write",       "Write I/O port",          kWrite, false },

        { "z80_pin_set",        "Set pin",                 kWrite, false },
        { "z80_pin_set_at",     "Set pin at half-cycle",   kWrite, false },
        { "z80_pin_set_at_pc",  "Set pin at PC",           kWrite, false },

        { "z80_break_add",      "Add breakpoint",          kWrite, true  },
        { "z80_break_clear",    "Clear breakpoints",       kWrite, true  },

        { "z80_view_set",       "Set die view",            kWrite, true  },
        { "z80_view_grab",      "Export die image",        kHost,  false },

        { "z80_watchlist_add",  "Watch nets",              kWrite, false },
        { "z80_sample_window",  "Capture net samples",     kWipe,  false },

        { "z80_rename_net",     "Rename net",              kWrite, true  },
        { "z80_delete_net_name","Delete net name",         kWipe,  true  },

        { "z80_eval_js",        "Evaluate JavaScript",     kHost,  false },
    };

    for (const MetaRow &row : kMeta)
    {
        for (ToolDef &t : m_tools)
        {
            if (t.name != QLatin1String(row.name))
                continue;
            t.title     = QString::fromLatin1(row.title);
            t.hints     = row.hints;
            t.reentrant = row.reentrant;
            break;
        }
    }

    // Output schemas. Only tools whose payload has a settled shape declare one: a client validates
    // structuredContent against it, so a schema that drifts from the handler is worse than none.
    struct SchemaRow { const char *name; QJsonObject schema; };
    const QJsonObject netArray = schemaArray(schemaNetValue(), "One entry per requested net");

    const SchemaRow kSchemas[] = {
        { "z80_now", schemaObject({
              {"hc",      schemaInt("Current half-cycle count")},
              {"pc",      schemaInt("Program counter")},
              {"mt",      schemaString("M/T state, for example M1T3")},
              {"clk",     schemaInt("Clock level, 0 or 1")},
              {"running", schemaBool("True while the simulation is advancing")},
          }, {"hc", "pc"}) },

        { "z80_net_read", schemaObject({ {"results", netArray} }, {"results"}) },

        { "z80_run", schemaObject({
              {"ok",            schemaBool()},
              {"hc_start",      schemaInt("Half-cycle count before the run")},
              {"hc_final",      schemaInt("Half-cycle count after the run")},
              {"pc_final",      schemaInt("Program counter where the run stopped")},
              {"stopped_by",    schemaString("count, pc, cycle, net, timeout or stop")},
              {"still_running", schemaBool("True if the chip is somehow still advancing")},
          }, {"ok", "hc_final", "stopped_by"}) },

        { "z80_mem_read", schemaObject({
              {"addr",  schemaInt("First address read")},
              {"len",   schemaInt("Number of bytes returned")},
              {"bytes", schemaArray(schemaInt(), "Byte values, one per address")},
          }, {"addr", "len", "bytes"}) },

        { "z80_net_find", schemaObject({
              {"total",   schemaInt("Number of matches returned")},
              {"matches", schemaArray(schemaObject({
                              {"id",   schemaInt("Net number")},
                              {"name", schemaString("Net name")},
                          }, {"id", "name"}))},
          }, {"total", "matches"}) },
    };

    for (const SchemaRow &row : kSchemas)
    {
        for (ToolDef &t : m_tools)
        {
            if (t.name != QLatin1String(row.name))
                continue;
            t.outputSchema = row.schema;
            break;
        }
    }
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
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndReset(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        uint hc = ::controller.doReset();
        QJsonObject r; r["ok"] = true; r["hc"] = qint64(hc);
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndRun(const QJsonObject &a, QString &)
{
    const int halfcycles = intArg(a, "halfcycles", 0);
    // The wait runs a nested event loop on the GUI thread, so an unbounded wait is an unrecoverable
    // freeze: a zero or negative timeout is clamped rather than honoured.
    const int askedTimeout = intArg(a, "timeout_ms", 30000);
    const int timeoutMs  = (askedTimeout > 0) ? qMin(askedTimeout, 600000) : 30000;
    QJsonObject until = a.value("until").toObject();
    const bool wantPc    = until.contains("pc");
    const bool wantCycle = until.contains("cycle");
    const bool wantNet   = until.contains("net");
    const int untilPc    = intArg(until, "pc", -1);
    const int untilCycle = intArg(until, "cycle", -1);
    const QString untilNetName = strArg(until, "net");
    const int untilValue = intArg(until, "value", 0);

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        // Validate every condition before arming anything. Returning early from between two arming
        // calls would leave a breakpoint set that silently aborts an unrelated later run.
        net_t nid = 0;
        if (wantNet)
        {
            QString nerr;
            nid = resolveNetChecked(until.value("net"), nerr);
            if (nid == 0)
                return errorResult(QString("z80_run until.net: %1").arg(nerr));
            if ((untilValue != 0) && (untilValue != 1))
                return errorResult(QString("z80_run until.value must be 0 or 1, got %1.").arg(untilValue));
        }
        // The trickbox holds the cycle stop in 16 bits and compares it for exact equality, so a
        // larger value would wrap and then fire at the wrong time or never at all.
        if (wantCycle && ((untilCycle < 0) || (untilCycle > 0xFFFF)))
            return errorResult(QString("z80_run until.cycle must be 0..65535, got %1.").arg(untilCycle));

        if (wantCycle)
            ::controller.getTrickbox().stopAt(quint16(untilCycle));
        if (wantNet)
            ::controller.getTrickbox().breakWhen(nid, quint8(untilValue));

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

        // Arm the wait before starting the run. A run of one or two half-cycles completes inside
        // doRunsim() and emits onRunStopped from there, so connecting afterwards would miss the
        // only wakeup this loop ever gets and leave single-stepping waiting out the whole timeout.
        QEventLoop loop;
        QTimer timeoutTimer;
        bool timedOut = false;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&::controller, &ClassController::onRunStopped, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&loop, &timedOut]() {
            timedOut = true;
            loop.quit();
        });
        timeoutTimer.start(timeoutMs);

        // Start the run: INT_MAX means "run forever until a breakpoint or stop"
        // (ClassController::doRunsim treats this as "Starting simulation")
        uint ticks = halfcycles > 0 ? uint(halfcycles) : uint(INT_MAX);
        const uint hcStart = ::controller.getSimZ80().getCurrentHCycle();
        ::controller.doRunsim(ticks);

        // Only wait if the run is still in flight. A short run has already finished by now.
        if (::controller.isSimRunning())
            loop.exec();
        pcPoll.stop();
        timeoutTimer.stop();

        // A timeout means we gave up waiting, not that the chip stopped. Halt it here: leaving the
        // worker running would keep rewriting the netlist under every value reported below.
        if (timedOut)
        {
            ::controller.doRunsim(0);
            // Only claim a timeout when nothing else already said why the run ended: the PC poll
            // may have hit its target and simply not finished stopping in time.
            if (stoppedBy.isEmpty())
                stoppedBy = "timeout";
        }

        if (stoppedBy.isEmpty())
        {
            const uint hcEnd = ::controller.getSimZ80().getCurrentHCycle();
            stoppedBy = ((halfcycles > 0) && ((hcEnd - hcStart) >= uint(halfcycles))) ? "count"
                      : wantCycle ? "cycle"
                      : wantNet   ? "net"
                      : "stop";
        }

        // Best-effort clear of cycle/net breakpoints we set
        if (wantCycle)
            ::controller.getTrickbox().stopAt(0);
        if (wantNet)
            ::controller.getTrickbox().breakWhen(0, 0);

        QJsonObject r;
        r["ok"]          = true;
        r["hc_start"]    = qint64(hcStart);
        r["hc_final"]    = qint64(::controller.getSimZ80().getCurrentHCycle());
        r["pc_final"]    = int(::controller.getSimZ80().getPC());
        r["stopped_by"]  = stoppedBy;
        r["still_running"] = ::controller.isSimRunning();
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndStop(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.doRunsim(0);
        QJsonObject r;
        r["ok"] = true;
        r["hc"] = qint64(::controller.getSimZ80().getCurrentHCycle());
        return structuredResult(r);
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
        return structuredResult(r);
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
            QString neterr;
            net_t id = resolveNetChecked(v, neterr);
            if (id == 0)
            {
                // One bad name should not throw away the readings for the rest of the batch, so the
                // entry carries its own reason instead of failing the whole call.
                e["id"]    = 0;
                e["name"]  = v.isString() ? v.toString() : QString::number(v.toInt());
                e["error"] = neterr;
                out.append(e);
                continue;
            }
            e["id"]    = int(id);
            e["name"]  = nl.get(id);
            e["value"] = readBitByNum(id);
            out.append(e);
        }
        QJsonObject r; r["results"] = out;
        return structuredResult(r);
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
        return structuredResult(r);
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
        return structuredResult(r);
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
            e["gate"]   = int(nl.getTransGate(t));
            e["source"] = int(c1);
            e["drain"]  = int(c2);
            e["on"]     = nl.isTransOn(t);
            out.append(e);
        }
        QJsonObject r; r["results"] = out;
        return structuredResult(r);
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
        return structuredResult(r);
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
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndNetInfo(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassNetlist &nl = ::controller.getNetlist();
        QString neterr;
        net_t id = resolveNetChecked(a.value("net"), neterr);
        if (id == 0) return errorResult(neterr);
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

        ClassVisual &cv = ::controller.getChip();
        const segvdef *sv = cv.getSegment(id);
        if (sv && !sv->path.isEmpty())
        {
            QRect b = sv->path.boundingRect().toAlignedRect();
            if (!b.isEmpty())
            {
                QJsonArray bb;
                bb.append(b.left()); bb.append(b.top()); bb.append(b.width()); bb.append(b.height());
                r["bbox"] = bb;
            }
        }
        return structuredResult(r);
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
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndEquation(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QString neterr;
        net_t id = resolveNetChecked(a.value("net"), neterr);
        if (id == 0) return errorResult(neterr);
        QJsonObject r;
        r["id"]   = int(id);
        r["name"] = ::controller.getNetlist().get(id);
        r["expr"] = ::controller.getNetlist().equation(id);
        return structuredResult(r);
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
        return structuredResult(r);
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
        return structuredResult(r);
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
        return structuredResult(r);
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
        return structuredResult(r);
    });
}

// ===========================================================================
// Pin control
// ===========================================================================

/*
 * The trickbox matches pin names case-sensitively against a lower-case table, so a caller using the
 * conventional upper-case spelling used to be accepted, do nothing, and still report success.
 * Normalising here makes both spellings work and turns an unknown pin into a real error.
 */
static const QStringList &knownPins()
{
    static const QStringList pins { "int", "nmi", "busrq", "wait", "reset" };
    return pins;
}

static bool normalizePin(const QString &in, QString &out, QString &err)
{
    out = in.trimmed().toLower();
    if (knownPins().contains(out))
        return true;
    err = QString("Unknown pin \"%1\". Valid pins are: %2.")
              .arg(in, knownPins().join(QStringLiteral(", ")));
    return false;
}

QJsonValue ClassMcpTools::hndPinSet(const QJsonObject &a, QString &err)
{
    const QString pin = strArg(a, "pin");
    const int value = intArg(a, "value", -1);
    if (pin.isEmpty()) { err = "Missing 'pin'."; return {}; }
    if ((value < 0) || (value > 2)) { err = QString("'value' must be 0, 1 or 2 (hi-Z), got %1.").arg(value); return {}; }
    QString norm;
    if (!normalizePin(pin, norm, err)) return {};

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.getTrickbox().set(norm, quint8(value));
        QJsonObject r; r["ok"] = true; r["pin"] = norm; r["value"] = value;
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndPinSetAt(const QJsonObject &a, QString &err)
{
    const QString pin = strArg(a, "pin");
    const int at_hc = intArg(a, "at_hc", -1);
    const int hold  = intArg(a, "hold", -1);
    if (pin.isEmpty() || at_hc < 0 || hold < 0) { err = "Missing or negative 'pin', 'at_hc' or 'hold'."; return {}; }
    if ((at_hc > 0xFFFF) || (hold > 0xFFFF))
        { err = "'at_hc' and 'hold' must be 0..65535; the trickbox stores them as 16 bits."; return {}; }
    QString norm;
    if (!normalizePin(pin, norm, err)) return {};

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.getTrickbox().setAt(norm, quint16(at_hc), quint16(hold));
        QJsonObject r; r["ok"] = true; r["pin"] = norm; r["at_hc"] = at_hc; r["hold"] = hold;
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndPinSetAtPc(const QJsonObject &a, QString &err)
{
    const QString pin = strArg(a, "pin");
    const int pc   = intArg(a, "pc", -1);
    const int hold = intArg(a, "hold", -1);
    if (pin.isEmpty() || pc < 0 || hold < 0) { err = "Missing or negative 'pin', 'pc' or 'hold'."; return {}; }
    if ((pc > 0xFFFF) || (hold > 0xFFFF))
        { err = "'pc' and 'hold' must be 0..65535."; return {}; }
    QString norm;
    if (!normalizePin(pin, norm, err)) return {};

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ::controller.getTrickbox().setAtPC(norm, quint16(pc), quint16(hold));
        QJsonObject r; r["ok"] = true; r["pin"] = norm; r["pc"] = pc; r["hold"] = hold;
        return structuredResult(r);
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
        return structuredResult(r);
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
        return structuredResult(r);
    });
}

// ===========================================================================
// Interactive image view
// ===========================================================================

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
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndViewGrab(const QJsonObject &, QString &err)
{
    // Triggers the host-side "Export PNG..." save dialog by invoking
    // WidgetImageView::onPng() on a visible image view. The dialog runs on
    // the user's machine; the user picks the destination. No PNG bytes are
    // returned over MCP.
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QWidgetList widgets = QApplication::topLevelWidgets();
        QWidget *found = nullptr;
        for (QWidget *w : widgets)
        {
            QWidget *child = w->findChild<QWidget*>("widgetImageView");
            if (child) { found = child; break; }
        }
        if (!found) { err = "No DockImageView found"; return QJsonValue{}; }
        QMetaObject::invokeMethod(found, "onPng", Qt::QueuedConnection);
        QJsonObject r;
        r["ok"] = true;
        r["note"] = "Triggered the image-view 'Export PNG' dialog on the host.";
        return structuredResult(r);
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
        return structuredResult(r);
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
        return structuredResult(r);
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
        return structuredResult(r);
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

        // Arm the wait first, then block only if the run is still in flight. A short run finishes
        // inside doRunsim() and exec() clears a wakeup posted before it, so the guard is what makes
        // the short case work rather than sitting out the whole timeout.
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&::controller, &ClassController::onRunStopped, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeoutTimer.start(30000);

        ::controller.doRunsim(uint(halfcycles));
        if (::controller.isSimRunning())
            loop.exec();
        timeoutTimer.stop();

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
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndEquationTree(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QString neterr;
        net_t id = resolveNetChecked(a.value("net"), neterr);
        if (id == 0) return errorResult(neterr);
        QJsonObject r = ::controller.getNetlist().equationTreeJson(id);
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndNetDrivers(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QString neterr;
        net_t id = resolveNetChecked(a.value("net"), neterr);
        if (id == 0) return errorResult(neterr);
        QJsonObject r = ::controller.getNetlist().netDriversJson(id);
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndRenameNet(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QString neterr;
        net_t id = resolveNetChecked(a.value("net"), neterr);
        if (id == 0) return errorResult(neterr);
        QString name = a.value("name").toString().trimmed();
        if (name.isEmpty()) return errorResult("The new name is empty. Use z80_delete_net_name to clear a name.");
        QString oldName = ::controller.getNetlist().get(id);
        if (oldName.isEmpty()) return errorResult("That net has no name yet. Assign one with z80_eval_js setNetName().");
        net_t taken = ::controller.getNetlist().get(name);
        if ((taken != 0) && (taken != id)) return errorResult(QString("The name is already on net %1. Pick another name or rename that net first.").arg(int(taken)));
        ::controller.renameNet(name, id);
        QJsonObject r;
        r["id"] = int(id);
        r["old_name"] = oldName;
        r["new_name"] = ::controller.getNetlist().get(id);
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndDeleteNetName(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QString neterr;
        net_t id = resolveNetChecked(a.value("net"), neterr);
        if (id == 0) return errorResult(neterr);
        QString oldName = ::controller.getNetlist().get(id);
        if (oldName.isEmpty()) return errorResult("That net has no name to delete.");
        ::controller.deleteNetName(id);
        QJsonObject r;
        r["id"] = int(id);
        r["deleted_name"] = oldName;
        return structuredResult(r);
    });
}

// ===========================================================================
// Resources
// ===========================================================================

/*
 * The die's topology is fixed for the life of the process: nets, transistors and what drives what
 * cannot change while the app runs. Exposing that as resources lets a client fetch it once and keep
 * it, instead of spending a tool round trip every time a question touches it.
 */

static QJsonObject makeResourceDef(const QString &uri, const QString &name,
                                   const QString &title, const QString &description)
{
    QJsonObject o;
    o["uri"]         = uri;
    o["name"]        = name;
    o["title"]       = title;
    o["description"] = description;
    o["mimeType"]    = "application/json";
    return o;
}

static QJsonObject makeTemplateDef(const QString &uriTemplate, const QString &name,
                                   const QString &title, const QString &description)
{
    QJsonObject o;
    o["uriTemplate"] = uriTemplate;
    o["name"]        = name;
    o["title"]       = title;
    o["description"] = description;
    o["mimeType"]    = "application/json";
    return o;
}

QJsonArray ClassMcpTools::resourcesList() const
{
    QJsonArray out;
    out.append(makeResourceDef("z80://nets", "nets", "Named nets",
        "Every named net on the die as {id, name}. Fetch this once instead of guessing net names."));
    out.append(makeResourceDef("z80://pins", "pins", "Chip pins",
        "The external pin nets and their current logic values."));
    return out;
}

QJsonArray ClassMcpTools::resourceTemplatesList() const
{
    QJsonArray out;
    out.append(makeTemplateDef("z80://net/{ref}", "net", "Net details",
        "Topology and current value of one net. {ref} is a net name or numeric id."));
    out.append(makeTemplateDef("z80://drivers/{ref}", "drivers", "Net drivers",
        "The transistors that pull one net. {ref} is a net name or numeric id."));
    out.append(makeTemplateDef("z80://fanout/{ref}", "fanout", "Net fanout",
        "What one net gates. {ref} is a net name or numeric id."));
    out.append(makeTemplateDef("z80://equation/{ref}", "equation", "Net logic equation",
        "The boolean equation behind one net. {ref} is a net name or numeric id."));
    return out;
}

QJsonValue ClassMcpTools::readResource(const QString &uri, QString &err)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassNetlist &nl = ::controller.getNetlist();
        QJsonValue payload;

        if (uri == QLatin1String("z80://nets"))
        {
            QJsonArray nets;
            for (net_t i = 1; i < MAX_NETS; i++)
            {
                const QString &n = nl.get(i);
                if (n.isEmpty())
                    continue;
                QJsonObject e; e["id"] = int(i); e["name"] = n;
                nets.append(e);
            }
            QJsonObject o; o["count"] = nets.size(); o["nets"] = nets;
            payload = o;
        }
        else if (uri == QLatin1String("z80://pins"))
        {
            static const char *kPins[] = { "clk", "_reset", "_int", "_nmi", "_busrq", "_wait",
                                           "_m1", "_mreq", "_iorq", "_rd", "_wr", "_rfsh",
                                           "_halt", "_busak" };
            QJsonArray pins;
            for (const char *p : kPins)
            {
                const net_t id = nl.get(QString::fromLatin1(p));
                if (id == 0)
                    continue;
                QJsonObject e;
                e["id"]    = int(id);
                e["name"]  = QString::fromLatin1(p);
                e["value"] = readBitByNum(id);
                pins.append(e);
            }
            QJsonObject o; o["pins"] = pins;
            payload = o;
        }
        else
        {
            // The templated forms. Everything after the last slash is the net reference.
            static const QStringList kPrefixes { "z80://net/", "z80://drivers/", "z80://fanout/",
                                                 "z80://equation/" };
            QString kind, ref;
            for (const QString &pfx : kPrefixes)
            {
                if (!uri.startsWith(pfx))
                    continue;
                kind = pfx.mid(6).chopped(1);        // strip the "z80://" and the trailing slash
                ref  = QUrl::fromPercentEncoding(uri.mid(pfx.size()).toUtf8());
                break;
            }
            if (kind.isEmpty())
            {
                err = QString("Unknown resource URI \"%1\". Call resources/list and "
                              "resources/templates/list for what this server serves.").arg(uri);
                return QJsonValue();
            }

            QString neterr;
            const net_t id = resolveNetChecked(QJsonValue(ref), neterr);
            if (id == 0)
            {
                err = neterr;
                return QJsonValue();
            }

            QJsonObject o;
            o["id"]   = int(id);
            o["name"] = nl.get(id);
            if (kind == QLatin1String("net"))
            {
                o["value"]     = readBitByNum(id);
                o["pullup"]    = nl.isNetPulledUp(id);
                o["orphan"]    = nl.isNetOrphan(id);
                o["gateless"]  = nl.isNetGateless(id);
            }
            else if (kind == QLatin1String("drivers"))
                o["drivers"] = nl.netDriversJson(id);
            else if (kind == QLatin1String("fanout"))
            {
                QJsonArray gated, connected;
                for (tran_t t : nl.getGatedTransistors(id))
                    gated.append(int(t));
                for (tran_t t : nl.getConnectedTransistors(id))
                    connected.append(int(t));
                o["gates"]    = gated;
                o["connects"] = connected;
            }
            else if (kind == QLatin1String("equation"))
                o["equation"] = nl.equation(id);
            payload = o;
        }

        QJsonObject content;
        content["uri"]      = uri;
        content["mimeType"] = "application/json";
        content["text"]     = QString::fromUtf8(QJsonDocument(payload.toObject()).toJson(QJsonDocument::Compact));
        QJsonObject r;
        r["contents"] = QJsonArray{ content };
        return r;
    });
}

// ===========================================================================
// Prompts
// ===========================================================================

/*
 * The investigations this project repeats. Shipping them as prompts means the method travels with
 * the server instead of being reconstructed from memory at the start of every session.
 */

struct PromptDef
{
    const char *name;
    const char *title;
    const char *description;
    const char *argName;
    const char *argDescription;
    const char *body;
};

static const PromptDef kPrompts[] = {
    { "trace-net", "Trace what drives a net",
      "Walk back from a net to the transistors and PLA rows that drive it.",
      "net", "Net name or numeric id to trace",
      "Trace what drives net %1 in the Z80 netlist.\n\n"
      "1. Read z80://net/%1 for its current value and topology.\n"
      "2. Read z80://drivers/%1 for the transistors that pull it, and note each driver's gate net.\n"
      "3. Read z80://equation/%1 for the boolean form, and z80_equation_tree for the structure.\n"
      "4. For each gate net that is a pla* row, say which instruction class that row decodes.\n"
      "5. State the conclusion as: net %1 is HIGH when <condition>, driven by <drivers>.\n\n"
      "Verify every claim against the simulator before stating it." },

    { "characterise-pla-row", "Characterise a PLA row",
      "Determine which opcodes fire a PLA row and what it controls.",
      "row", "PLA row name, for example pla65",
      "Characterise PLA row %1.\n\n"
      "1. Read z80://fanout/%1 to see what the row gates.\n"
      "2. Classify each net it reaches: is it an inverter, a NOR, a pass transistor, or dead?\n"
      "3. Prime memory with candidate opcodes using z80_mem_write, reset, and run with z80_run to\n"
      "   find which opcodes actually assert %1. Use z80_watchlist_add plus z80_waveform_window to\n"
      "   see when it asserts within the M-cycle.\n"
      "4. Report the opcode pattern, the state lines that gate it, and what it controls.\n\n"
      "Do not claim decode behaviour you have not observed in a capture." },

    { "capture-opcode-timing", "Capture opcode timing",
      "Capture the half-cycle timing of one instruction from reset.",
      "opcode", "Opcode bytes in hex, for example 'DD 7E 05'",
      "Capture the half-cycle timing of opcode %1.\n\n"
      "1. z80_mem_write the opcode bytes at address 0, followed by 76 (HALT).\n"
      "2. z80_watchlist_add the nets you need: clk, m1..m5, t1..t6, _mreq, _rd, _wr, _rfsh, and the\n"
      "   pla rows you expect to fire.\n"
      "3. z80_reset, then z80_run one half-cycle at a time, or run to a known stop and read back the\n"
      "   window with z80_waveform_window.\n"
      "4. Build a table of M-cycle, T-state, clock level, external pins and internal dataflow.\n\n"
      "Remember the clock is counted in half-cycles: one T-state is two of them." },

    { "find-latch", "Find the latch behind a signal",
      "Identify the storage element that holds a signal across clock phases.",
      "net", "Net name or numeric id that appears to be latched",
      "Find the latch that holds net %1.\n\n"
      "1. Read z80://drivers/%1 and look for a pass transistor gated by a clock phase.\n"
      "2. Follow the other side of that pass transistor; a dynamic latch is a gate capacitance with\n"
      "   no pull-up, a static one is a cross-coupled pair.\n"
      "3. Confirm by capture: watch %1 and the clock, run across a clock edge, and check the value\n"
      "   survives the phase in which its driver is cut off.\n"
      "4. Report the topology, the transistor ids, and which clock phase samples and which holds.\n\n"
      "State plainly if the evidence does not support calling it a latch." },
};

QJsonArray ClassMcpTools::promptsList() const
{
    QJsonArray out;
    for (const PromptDef &p : kPrompts)
    {
        QJsonObject arg;
        arg["name"]        = QString::fromLatin1(p.argName);
        arg["description"] = QString::fromLatin1(p.argDescription);
        arg["required"]    = true;

        QJsonObject o;
        o["name"]        = QString::fromLatin1(p.name);
        o["title"]       = QString::fromLatin1(p.title);
        o["description"] = QString::fromLatin1(p.description);
        o["arguments"]   = QJsonArray{ arg };
        out.append(o);
    }
    return out;
}

QJsonValue ClassMcpTools::getPrompt(const QString &name, const QJsonObject &args, QString &err)
{
    for (const PromptDef &p : kPrompts)
    {
        if (name != QLatin1String(p.name))
            continue;

        const QString value = args.value(QString::fromLatin1(p.argName)).toString();
        if (value.isEmpty())
        {
            err = QString("Prompt \"%1\" needs the \"%2\" argument: %3")
                      .arg(name, QString::fromLatin1(p.argName), QString::fromLatin1(p.argDescription));
            return {};
        }

        QJsonObject text;
        text["type"] = "text";
        text["text"] = QString::fromLatin1(p.body).arg(value);

        QJsonObject msg;
        msg["role"]    = "user";
        msg["content"] = text;

        QJsonObject r;
        r["description"] = QString::fromLatin1(p.description);
        r["messages"]    = QJsonArray{ msg };
        return r;
    }
    err = QString("Unknown prompt \"%1\". Call prompts/list for the available names.").arg(name);
    return {};
}

// ===========================================================================
// Argument completion
// ===========================================================================

/*
 * A model addressing a net by name gets it wrong often, and the recovery costs a round trip. The
 * spec scopes completion to prompt arguments and resource-template variables, which is exactly
 * where this server's net-name arguments live, so completion closes most of that gap.
 */
QJsonObject ClassMcpTools::complete(const QJsonObject &ref, const QString &argName,
                                    const QString &value) const
{
    Q_UNUSED(argName)

    const QString kind = ref.value("type").toString();
    const bool wantsNet = (kind == QLatin1String("ref/resource"))
                       || (kind == QLatin1String("ref/prompt"));

    QStringList names;
    if (wantsNet && !value.isEmpty())
    {
        const QString needle = value.toLower();
        ClassNetlist &nl = ::controller.getNetlist();
        for (net_t i = 1; (i < MAX_NETS) && (names.size() < 100); i++)
        {
            const QString &n = nl.get(i);
            if (!n.isEmpty() && n.toLower().startsWith(needle))
                names.append(n);
        }
    }

    QJsonArray values;
    for (const QString &n : names)
        values.append(n);

    QJsonObject completion;
    completion["values"]  = values;
    completion["total"]   = names.size();
    completion["hasMore"] = (names.size() >= 100);

    QJsonObject r;
    r["completion"] = completion;
    return r;
}
