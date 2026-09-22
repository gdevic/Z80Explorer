#include "ClassMcpTools.h"
#include "ClassAnnotate.h"
#include "ClassController.h"
#include "ClassMcpThreading.h"
#include "ClassNetlist.h"
#include "ClassRenderer.h"
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
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QTimer>
#include <QWidget>
#include <climits>

// How many targets one batched query answers. The plain lookups are a handful of array reads each,
// so the cap is only there to bound the reply. An equation walks the netlist recursively per target,
// which is orders of magnitude dearer, so that form gets its own much smaller cap.
// Render limits. The inline cap is deliberately well under a megabyte: an inline image is base64
// inside the JSON reply and counts against the client's per-call output budget, and the API
// downscales anything much larger anyway, so past this size a file is the better answer.
static const int MCP_RENDER_INLINE_MAX = 350000;
static const int MCP_RENDER_MAX_SIDE   = 4096;

// Where a render lands when it is too large to inline. Not the resource tree and not the repo: the
// application chdir's into resource/, so anything relative would end up inside the checkout.
static QString renderOutputDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/z80explorer-render";
}

static const int MCP_BATCH_CAP      = 256;
static const int MCP_BATCH_CAP_TREE = 16;

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

QJsonValue ClassMcpTools::imageResult(const QByteArray &pngBytes, const QJsonValue &jsonPayload)
{
    QJsonDocument doc(jsonPayload.isObject() ? jsonPayload.toObject()
                                             : QJsonObject{{"value", jsonPayload}});
    QJsonObject text;
    text["type"] = "text";
    text["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    QJsonArray content;
    if (!pngBytes.isEmpty())
    {
        QJsonObject img;
        img["type"]     = "image";
        img["data"]     = QString::fromLatin1(pngBytes.toBase64());
        img["mimeType"] = "image/png";
        content.append(img);
    }
    content.append(text);

    QJsonObject r;
    r["content"]           = content;
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

/*
 * ClassWatch keys its entries by string, and resolves a net name, a bus name or a net number. This
 * turns one `nets` entry into that key: a name passes through, a numeric reference becomes its
 * canonical decimal string. Anything that names nothing is rejected here, because updateWatchlist()
 * drops an unresolvable entry without saying so, which would leave the caller a missing column
 * instead of an error.
 */
QString ClassMcpTools::resolveWatchKey(const QJsonValue &v, QString &err) const
{
    ClassNetlist &nl = ::controller.getNetlist();

    if (v.isDouble())
    {
        const int n = v.toInt(-1);
        if ((n <= 0) || (n >= int(MAX_NETS)))
        {
            err = QString("Net id %1 is out of range; valid ids are 1..%2.").arg(n).arg(MAX_NETS - 1);
            return {};
        }
        return QString::number(n);
    }

    if (!v.isString())
    {
        err = QStringLiteral("A watched net must be given as a string name or an integer id.");
        return {};
    }

    QString s = v.toString().trimmed();  // Not const: ClassNetlist::getBus() takes a QString&
    if (s.isEmpty())
    {
        err = QStringLiteral("A watched net name cannot be empty.");
        return {};
    }
    if (nl.get(s) || !nl.getBus(s).isEmpty())
        return s;                       // An existing net name, or a bus name

    bool isNum = false;
    const uint num = s.toUInt(&isNum);
    if (isNum)
    {
        if ((num == 0) || (num >= MAX_NETS))
        {
            err = QString("Net id %1 is out of range; valid ids are 1..%2.").arg(num).arg(MAX_NETS - 1);
            return {};
        }
        return QString::number(num);
    }

    const QStringList near = nearestNetNames(s);
    err = near.isEmpty()
        ? QString("Unknown net or bus \"%1\". Use z80_net_find to search net names.").arg(s)
        : QString("Unknown net or bus \"%1\". Did you mean: %2? Use z80_net_find to search.")
              .arg(s, near.join(QStringLiteral(", ")));
    return {};
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
static QJsonObject schemaNumber(const QString &desc = {}, double lo = 0.0, double hi = 0.0)
{
    QJsonObject s; s["type"] = "number";
    if (!desc.isEmpty()) s["description"] = desc;
    if (hi > lo) { s["minimum"] = lo; s["maximum"] = hi; }
    return s;
}
static QJsonObject schemaColorMap(const QString &desc)
{
    QJsonObject s; s["type"] = "object"; s["description"] = desc;
    s["additionalProperties"] = QJsonObject{{"type", "string"}};
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
static QJsonObject schemaObject(const QJsonObject &props, const QJsonArray &required = {}, bool closed = false)
{
    QJsonObject s; s["type"] = "object"; s["properties"] = props;
    if (!required.isEmpty()) s["required"] = required;
    // Closing the object turns a misspelled argument name into a client-side rejection instead of
    // a call that quietly ignores it. Worth it on a tool that writes files.
    if (closed) s["additionalProperties"] = false;
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
        "Get structural information about a net: name, driving/driven transistors, pullup, bounding "
        "box. Pass 'nets' with a list instead of 'net' to answer up to 256 in one call; each row "
        "then carries its own 'error' if that entry could not be resolved.",
        schemaObject({
            {"net",  schemaNetRef()},
            {"nets", schemaArray(schemaNetRef(), "Up to 256 nets, answered in one call")},
        }),
        [this](const QJsonObject &a, QString &err) { return hndNetInfo(a, err); }
    });

    registerTool({
        "z80_trans_info",
        "Get information about a transistor: gate/source/drain nets, box, current on/off state. "
        "Pass 'ids' with a list instead of 'id' to answer up to 256 in one call.",
        schemaObject({
            {"id",  schemaInt("Transistor id")},
            {"ids", schemaArray(schemaInt(), "Up to 256 transistor ids, answered in one call")},
        }),
        [this](const QJsonObject &a, QString &err) { return hndTransInfo(a, err); }
    });

    registerTool({
        "z80_equation",
        "Return the logic equation driving a net, as the raw parse tree flattened to one line. "
        "Pass 'nets' instead of 'net' to answer several in one call.",
        schemaObject({
            {"net",  schemaNetRef()},
            {"nets", schemaArray(schemaNetRef(), "Up to 16 nets; each one is a full tree walk")},
        }),
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
        "z80_die_info",
        "Describe the die image and the coordinate system every spatial answer uses, plus the layer "
        "names z80_view_render accepts, the net and transistor draw modes, and the net value "
        "encoding. Read this once instead of re-deriving the coordinate conventions.",
        schemaNoArgs(),
        [this](const QJsonObject &a, QString &err) { return hndDieInfo(a, err); }
    });

    registerTool({
        "z80_view_render",
        "Render a region of the die to a PNG the caller can see. The region is given in die pixels; "
        "call z80_die_info for the coordinate system and the layer names. Small images come back "
        "inline as an image block, larger ones are written to a file whose path is returned. "
        "Overlays mirror the interactive view: active nets, transistors, latches, pull-ups, "
        "net-name labels and annotations, plus per-net colouring and explicit highlights.",
        schemaObject({
            {"x",                schemaInt("Left edge of the region, in die pixels")},
            {"y",                schemaInt("Top edge of the region, in die pixels")},
            {"w",                schemaInt("Region width in die pixels")},
            {"h",                schemaInt("Region height in die pixels")},
            {"scale",            schemaNumber("Output pixels per die pixel (default 1). Net-name "
                                              "labels only appear at 1.5 or above.", 0.02, 32.0)},
            {"out_w",            schemaInt("Output width; an alternative to scale")},
            {"out_h",            schemaInt("Output height; an alternative to scale")},
            {"layers",           schemaArray(schemaString(), "Layer names from z80_die_info. The "
                                             "first is the base, each further one XOR-blends over "
                                             "it, as the interactive view does. Default "
                                             "vss.vcc.nets.col")},
            {"nets",             schemaColorMap("Per-net colour overlay: key is a net name or id, "
                                                "value a colour such as #ff3c3c")},
            {"highlight_nets",   schemaArray(schemaNetRef(), "Painted bright yellow over everything")},
            {"highlight_trans",  schemaArray(schemaInt(), "Transistor ids, painted cyan")},
            {"highlight_rects",  schemaArray(schemaArray(schemaInt()), "[x,y,w,h] boxes, dashed red")},
            {"draw_nets",        schemaBool("Overlay active nets in one colour (default true)")},
            {"net_mode",         schemaInt("0 active, 1 pull-up, 2 gate-less, 3 gate-less no pull-up")},
            {"net_order",        schemaBool("Reverse the segment paint order (default false)")},
            {"draw_transistors", schemaBool("Overlay transistor outlines (default false)")},
            {"transistor_mode",  schemaInt("0 active, 1 single-flip, 2 sticky, 3 all")},
            {"draw_latches",     schemaBool("Overlay latch boxes and names (default false)")},
            {"draw_pullups",     schemaBool("Overlay pull-up symbols (default false)")},
            {"draw_net_names",   schemaBool("Overlay net-name labels; needs scale >= 1.5 (default false)")},
            {"draw_annotations", schemaBool("Overlay text annotations (default false)")},
            {"inline_max_bytes", schemaInt("Inline the PNG below this size, otherwise write a file "
                                           "and return its path")},
            {"path",             schemaString("Absolute path to write the PNG to; implies a file "
                                              "rather than an inline image")},
        }, {"x", "y", "w", "h"}),
        [this](const QJsonObject &a, QString &err) { return hndViewRender(a, err); }
    });

    registerTool({
        "z80_watchlist_add",
        "Append the given nets to the waveform watchlist so future "
        "z80_waveform_window calls return full sampled history for them. "
        "Each entry is a net name, a bus name, or a numeric net id, so a net "
        "that carries no name can be watched by number. Duplicates are "
        "ignored. Does NOT clear existing entries. An entry that names "
        "nothing fails the call and reports the closest existing names. "
        "The reply splits the outcome into 'added', 'skipped' (already "
        "present) and 'rejected'.",
        schemaObject({
            {"nets", schemaArray(schemaNetRef(), "Net names, bus names or numeric ids to append")},
        }, {"nets"}),
        [this](const QJsonObject &a, QString &err) { return hndWatchlistAdd(a, err); }
    });

    registerTool({
        "z80_watchlist_get",
        "Report the current waveform watchlist, the per-net history depth, and the half-cycle range "
        "that actually has recorded data. Call this before z80_waveform_window to see what an "
        "earlier session left behind and what range can be served without a capture.",
        schemaNoArgs(),
        [this](const QJsonObject &a, QString &err) { return hndWatchlistGet(a, err); }
    });

    registerTool({
        "z80_sample_window",
        "Convenience: add the requested nets to the watchlist, reset the sim, "
        "run N half-cycles, return the sampled table, then restore the prior "
        "watchlist. Use for single-shot captures without touching config. "
        "Each entry is a net name, a bus name, or a numeric net id, so a net "
        "that carries no name can be captured by number. An entry that names "
        "nothing fails the call and reports the closest existing names.",
        schemaObject({
            {"nets",       schemaArray(schemaNetRef(), "Net names, bus names or numeric ids to capture")},
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
            {"net",  schemaNetRef()},
            {"nets", schemaArray(schemaNetRef(), "Up to 16 nets; each one is a full tree walk")},
        }),
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
            {"net",  schemaNetRef()},
            {"nets", schemaArray(schemaNetRef(), "Up to 256 nets, answered in one call")},
        }),
        [this](const QJsonObject &a, QString &err) { return hndNetDrivers(a, err); }
    });

    registerTool({
        "z80_rename_net",
        "Rename a net that already has a name. The existing name is removed "
        "and replaced with the new one. Errors if the net has no existing "
        "name (use z80_eval_js setNetName(...) for first-time naming) or if "
        "the new name is already taken by another net. Held in memory until "
        "written by z80_save.",
        schemaObject({
            {"net",  schemaNetRef("Existing net name or numeric id")},
            {"name", schemaString("New name to assign")},
        }, {"net", "name"}),
        [this](const QJsonObject &a, QString &err) { return hndRenameNet(a, err); }
    });

    registerTool({
        "z80_delete_net_name",
        "Clear the name of a named net. Errors if the net has no name. "
        "Held in memory until written by z80_save.",
        schemaObject({
            {"net", schemaNetRef()},
        }, {"net"}),
        [this](const QJsonObject &a, QString &err) { return hndDeleteNetName(a, err); }
    });

    registerTool({
        "z80_save",
        "Write the user data held in memory out to disk, without closing the "
        "app. Safe to call while a simulation is running, so names and comments "
        "worked out during a long investigation can be kept without losing the "
        "state that produced them. With no arguments it saves everything that is "
        "currently available. Item ids: 'annotations', 'netnames' (net names, "
        "buses and their comments), 'colors', 'watchlist', and 'waveform-1' "
        "through 'waveform-4' (only the waveform views currently open). "
        "Results are split into 'saved', 'skipped' (the item deliberately wrote "
        "nothing, and says why) and 'failed'. To list the ids with their target "
        "paths, call z80_eval_js with the snippet "
        "print(JSON.stringify(saveList())).",
        schemaObject({
            {"items", schemaArray(schemaString("Save item id"),
                                  "Items to save; omit the key entirely to save every available item")},
        }, {}, /*closed*/ true),
        [this](const QJsonObject &a, QString &err) { return hndSave(a, err); }
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
    // Reads the simulator but may drop a PNG on the filesystem, and the image depends on where in
    // time the simulation currently is, so it is neither purely read-only nor idempotent.
    const ToolHints kRender { false, false, false, true  };  // renders; may write a file
    // Rewrites the user's data files in full, so it can overwrite hand edits; but repeating the
    // call with unchanged memory produces the same bytes and no further effect.
    const ToolHints kSave   { false, true,  true,  true  };  // writes user data; safe to retry

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
        // Not reentrant: getLogicTree() tracks visited nets and transistors, and the terminating-net
        // list, in file statics, so a second tree walk arriving inside one would corrupt both.
        { "z80_equation",       "Net logic equation",      kRead,  false },
        { "z80_equation_tree",  "Net logic tree",          kRead,  false },
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

        { "z80_die_info",       "Die and layer info",      kRead,  true  },
        { "z80_view_render",    "Render die region",       kRender, true },
        { "z80_watchlist_add",  "Watch nets",              kWrite, false },
        { "z80_watchlist_get",  "Read watchlist",          kRead,  true  },
        { "z80_sample_window",  "Capture net samples",     kWipe,  false },

        { "z80_rename_net",     "Rename net",              kWrite, true  },
        { "z80_delete_net_name","Delete net name",         kWipe,  true  },
        // Not reentrant: z80_sample_window swaps the watchlist for its own capture set and holds a
        // nested event loop, so a save arriving underneath it would persist that scratch list.
        { "z80_save",           "Save user data",          kSave,  false },

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

        { "z80_save", schemaObject({
              {"saved",   schemaArray(schemaObject({
                              {"id",    schemaString("Save item id")},
                              {"files", schemaArray(schemaString(), "Paths actually written")},
                          }, {"id", "files"}), "Items written, one entry each")},
              {"skipped", schemaArray(schemaObject({
                              {"id",     schemaString("Save item id")},
                              {"reason", schemaString("Why the item chose to write nothing")},
                          }, {"id", "reason"}), "Items that deliberately wrote nothing; not an error")},
              {"failed",  schemaArray(schemaObject({
                              {"id",     schemaString("Save item id")},
                              {"reason", schemaString("Why this item could not be saved")},
                          }, {"id", "reason"}), "Items that could not be written")},
          }, {"saved", "skipped", "failed"}) },
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
        if (toHc   < fromHc)         toHc   = fromHc;

        // from_hc / to_hc are an inclusive range, but getCurrentHCycle() is one past the last
        // half-cycle that was written. Work in a half-open range clamped to what actually exists,
        // so a to_hc of "now" does not read an unwritten slot and pick up the no-data sentinel.
        const uint endHc = qMin(uint(toHc) + 1, curHc);

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
                for (uint hc = uint(fromHc); hc < endHc; hc++)
                    arr.append(int(w.at(wp, hc)));
            }
            samples[name] = arr;
        }

        QJsonObject r;
        r["from_hc"] = fromHc;
        r["to_hc"]   = (endHc > uint(fromHc)) ? qint64(endHc - 1) : qint64(fromHc);
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

/*
 * Batch plumbing shared by the four query tools. The scalar argument keeps the flat reply it has
 * always had; the list argument returns one row per target under `results`. Both spellings live
 * side by side rather than as a scalar-or-array union on one key, so that tools/list documents the
 * list form explicitly and an existing caller's reply shape never changes.
 */
bool ClassMcpTools::batchArgs(const QJsonObject &a, const char *one, const char *many, int cap,
                              QJsonArray &items, bool &plural, QString &err)
{
    const bool hasOne  = a.contains(QLatin1String(one));
    const bool hasMany = a.contains(QLatin1String(many));
    if (hasOne && hasMany)
    {
        err = QString("Pass either '%1' or '%2', not both").arg(one, many);
        return false;
    }
    if (!hasOne && !hasMany)
    {
        err = QString("Missing argument: pass '%1' for one target or '%2' for a list").arg(one, many);
        return false;
    }
    plural = hasMany;
    if (hasMany)
    {
        items = a.value(QLatin1String(many)).toArray();
        if (items.isEmpty())
        {
            err = QString("'%1' is empty").arg(many);
            return false;
        }
        if (items.size() > cap)
        {
            err = QString("'%1' holds %2 entries; this tool answers at most %3 per call")
                      .arg(many).arg(items.size()).arg(cap);
            return false;
        }
    }
    else
        items = QJsonArray{ a.value(QLatin1String(one)) };
    return true;
}

QJsonValue ClassMcpTools::batchResult(const QJsonArray &items, bool plural,
                                      QJsonObject (ClassMcpTools::*build)(const QJsonValue &))
{
    if (!plural)
    {
        QJsonObject r = (this->*build)(items.at(0));
        // A scalar call reports a bad target as a tool error, which is what it did before batching.
        if (r.contains("error") && (r.size() == 1))
            return errorResult(r.value("error").toString());
        return structuredResult(r);
    }
    QJsonArray results;
    for (const QJsonValue &v : items)
        results.append((this->*build)(v));
    QJsonObject r;
    r["results"] = results;
    r["count"]   = results.size();
    return structuredResult(r);
}

QJsonObject ClassMcpTools::netInfoObject(const QJsonValue &ref)
{
    ClassNetlist &nl = ::controller.getNetlist();
    QString neterr;
    net_t id = resolveNetChecked(ref, neterr);
    if (id == 0)
    {
        QJsonObject e;
        e["error"] = neterr;
        return e;
    }
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
    return r;
}

QJsonObject ClassMcpTools::netDriversObject(const QJsonValue &ref)
{
    QString neterr;
    net_t id = resolveNetChecked(ref, neterr);
    if (id == 0)
    {
        QJsonObject e;
        e["error"] = neterr;
        return e;
    }
    return ::controller.getNetlist().netDriversJson(id);
}

QJsonObject ClassMcpTools::transInfoObject(const QJsonValue &ref)
{
    const int id = ref.toInt(-1);
    if ((id <= 0) || (id >= MAX_TRANS))
    {
        QJsonObject e;
        e["error"] = QString("Transistor id %1 is out of range 1..%2").arg(id).arg(MAX_TRANS - 1);
        return e;
    }
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
    return r;
}

QJsonObject ClassMcpTools::equationObject(const QJsonValue &ref)
{
    QString neterr;
    net_t id = resolveNetChecked(ref, neterr);
    if (id == 0)
    {
        QJsonObject e;
        e["error"] = neterr;
        return e;
    }
    QJsonObject r;
    r["id"]   = int(id);
    r["name"] = ::controller.getNetlist().get(id);
    r["expr"] = ::controller.getNetlist().equation(id);
    return r;
}

QJsonObject ClassMcpTools::equationTreeObject(const QJsonValue &ref)
{
    QString neterr;
    net_t id = resolveNetChecked(ref, neterr);
    if (id == 0)
    {
        QJsonObject e;
        e["error"] = neterr;
        return e;
    }
    return ::controller.getNetlist().equationTreeJson(id);
}

QJsonValue ClassMcpTools::hndNetInfo(const QJsonObject &a, QString &err)
{
    QJsonArray items;
    bool plural = false;
    if (!batchArgs(a, "net", "nets", MCP_BATCH_CAP, items, plural, err)) return {};

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        return batchResult(items, plural, &ClassMcpTools::netInfoObject);
    });
}

QJsonValue ClassMcpTools::hndTransInfo(const QJsonObject &a, QString &err)
{
    QJsonArray items;
    bool plural = false;
    if (!batchArgs(a, "id", "ids", MCP_BATCH_CAP, items, plural, err)) return {};

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        return batchResult(items, plural, &ClassMcpTools::transInfoObject);
    });
}

QJsonValue ClassMcpTools::hndEquation(const QJsonObject &a, QString &err)
{
    QJsonArray items;
    bool plural = false;
    if (!batchArgs(a, "net", "nets", MCP_BATCH_CAP_TREE, items, plural, err)) return {};

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        return batchResult(items, plural, &ClassMcpTools::equationObject);
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
        net_t id = resolveNetChecked(a.value("net"), err);
        if (id == 0) return QJsonValue{};

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
            const QString key = resolveWatchKey(v, err);
            if (key.isEmpty()) return QJsonValue{};
            if (current.contains(key)) { skipped.append(key); continue; }
            current.append(key);
            added.append(key);
        }
        w.updateWatchlist(current);

        // updateWatchlist() is the authority on what a watch can hold, so the reply reports the list
        // it produced rather than the list it was handed.
        const QStringList after = w.getWatchlist();
        QStringList rejected;
        for (int i = added.count() - 1; i >= 0; i--)
        {
            if (!after.contains(added.at(i)))
            {
                rejected.prepend(added.at(i));
                added.removeAt(i);
            }
        }

        QJsonObject r;
        r["added"]    = QJsonArray::fromStringList(added);
        r["skipped"]  = QJsonArray::fromStringList(skipped);   // already on the watchlist
        r["rejected"] = QJsonArray::fromStringList(rejected);  // resolved, but the watch did not take
        r["total"]    = int(w.getWatchlistLen());
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndDieInfo(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassVisual &cv = ::controller.getChip();
        ClassNetlist &nl = ::controller.getNetlist();
        const QImage &die0 = cv.getImage(0);

        QJsonObject image;
        image["width"]  = die0.width();
        image["height"] = die0.height();

        // The layer key characters are what the interactive view's setLayer()/addLayer() accept
        static const QString kLayerKeys = QStringLiteral("123456789abcdefghijk");
        const QStringList names = cv.getImageNames();
        QJsonArray layers;
        for (int i = 0; i < names.size(); i++)
        {
            QJsonObject e;
            e["index"]     = i;
            e["key"]       = (i < kLayerKeys.size()) ? QString(kLayerKeys.at(i)) : QString();
            e["name"]      = names.at(i);
            e["paintable"] = ClassRenderer::layerPaintable(cv.getImage(uint(i)));
            layers.append(e);
        }

        QJsonObject coords;
        coords["origin"] = "top-left";
        coords["x"]      = "increases rightward";
        coords["y"]      = "increases downward";
        coords["space"]  = "Every coordinate this server reports or accepts is a die image pixel. "
                           "z80_net_info bbox, z80_trans_info box, z80_view_set and z80_view_render "
                           "all use it, as [left, top, width, height] wherever a rect appears.";
        coords["segdefs_y_flip"] = "segdefs.js and transdefs.js store y inverted; the loader flips "
                                   "it as y_display = height - 1 - y_raw. layermap.bin is already "
                                   "top-left origin and is not flipped. Anything read straight out "
                                   "of those .js files therefore still needs the flip applied.";
        coords["view_pan"] = "The interactive view pans in normalized [0,1] texture coordinates, "
                             "but z80_view_set takes die pixels and converts.";

        QJsonArray netModes;
        for (const char *m : { "active", "pull-up", "gate-less", "gate-less no pull-up" })
            netModes.append(QString::fromLatin1(m));
        QJsonArray transModes;
        for (const char *m : { "active", "single-flip", "sticky", "all" })
            transModes.append(QString::fromLatin1(m));

        // The pin_t domain, in one place; the tool descriptions point here rather than repeat it
        QJsonObject values;
        values["0"] = "logic low";
        values["1"] = "logic high";
        values["2"] = "floating (hi-Z): nothing drives the net and it has no pull-up";
        values["3"] = "no data: nothing was recorded for that half-cycle, or it is outside the "
                      "range the ring buffer still holds";
        values["4"] = "the watch is a bus but was read through a net-shaped call";
        values["4294967295"] = "a bus read where at least one member net is floating";

        QJsonObject counts;
        counts["nets"]            = int(nl.getNetlistCount());
        counts["max_nets"]        = MAX_NETS;
        counts["max_transistors"] = MAX_TRANS;
        counts["pullups"]         = cv.getPullupCount();

        QJsonObject zoom;
        zoom["min"] = 0.1;
        zoom["max"] = 10.0;

        QJsonObject render;
        render["net_name_min_scale"] = 1.5;
        render["inline_max_bytes"]   = MCP_RENDER_INLINE_MAX;
        render["max_output_side"]    = MCP_RENDER_MAX_SIDE;
        render["output_dir"]         = QDir::toNativeSeparators(renderOutputDir());

        QJsonObject r;
        r["image"]            = image;
        r["layers"]           = layers;
        r["coords"]           = coords;
        r["net_modes"]        = netModes;
        r["transistor_modes"] = transModes;
        r["net_values"]       = values;
        r["counts"]           = counts;
        r["zoom"]             = zoom;
        r["render"]           = render;
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndViewRender(const QJsonObject &a, QString &err)
{
    if (!a.contains("x") || !a.contains("y") || !a.contains("w") || !a.contains("h"))
    {
        err = "Missing region: x, y, w and h are all required, in die pixels";
        return {};
    }
    const int x = intArg(a, "x", 0);
    const int y = intArg(a, "y", 0);
    const int w = intArg(a, "w", 0);
    const int h = intArg(a, "h", 0);
    if ((w < 1) || (h < 1))
    {
        err = QString("Region must be at least 1x1 die pixels; got %1x%2").arg(w).arg(h);
        return {};
    }

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        RenderSpec spec;
        spec.worldRect = QRectF(x, y, w, h);

        // Output size: an explicit out_w/out_h wins, then scale, then 1:1
        double scale = a.contains("scale") ? a.value("scale").toDouble(1.0) : 1.0;
        if (scale <= 0.0) scale = 1.0;
        int ow = a.contains("out_w") ? intArg(a, "out_w", 0) : int(qRound(w * scale));
        int oh = a.contains("out_h") ? intArg(a, "out_h", 0) : int(qRound(h * scale));
        if ((ow < 1) || (oh < 1))
            return errorResult(QString("Output size must be at least 1x1; got %1x%2").arg(ow).arg(oh));
        // Clamp rather than refuse: a caller asking for the whole die at 1:1 wants a picture, not an
        // error, and the metadata reports the size it actually got.
        const bool clamped = (ow > MCP_RENDER_MAX_SIDE) || (oh > MCP_RENDER_MAX_SIDE);
        ow = qMin(ow, MCP_RENDER_MAX_SIDE);
        oh = qMin(oh, MCP_RENDER_MAX_SIDE);
        spec.outputSize = QSize(ow, oh);

        for (const QJsonValue &v : a.value("layers").toArray())
        {
            if (!v.toString().isEmpty())
                spec.layerNames.append(v.toString());
        }

        spec.drawNets        = boolArg(a, "draw_nets", true);
        spec.netMode         = uint(qBound(0, intArg(a, "net_mode", 0), 3));
        spec.netOrder        = boolArg(a, "net_order", false);
        spec.drawTransistors = boolArg(a, "draw_transistors", false);
        spec.transistorMode  = uint(qBound(0, intArg(a, "transistor_mode", 0), 3));
        spec.drawLatches     = boolArg(a, "draw_latches", false);
        spec.drawPullups     = boolArg(a, "draw_pullups", false);
        spec.drawNetNames    = boolArg(a, "draw_net_names", false);
        spec.drawAnnotations = boolArg(a, "draw_annotations", false);

        for (const QJsonValue &v : a.value("highlight_nets").toArray())
        {
            QString neterr;
            net_t n = resolveNetChecked(v, neterr);
            if (n == 0) return errorResult(neterr);
            spec.highlightNets.append(n);
        }
        for (const QJsonValue &v : a.value("highlight_trans").toArray())
        {
            const int t = v.toInt(0);
            if ((t <= 0) || (t >= MAX_TRANS))
                return errorResult(QString("Transistor id %1 is out of range 1..%2")
                                       .arg(t).arg(MAX_TRANS - 1));
            spec.highlightTrans.append(tran_t(t));
        }
        for (const QJsonValue &v : a.value("highlight_rects").toArray())
        {
            const QJsonArray q = v.toArray();
            if (q.size() != 4)
                return errorResult("Each entry of highlight_rects must be [x, y, w, h]");
            spec.highlightRects.append(QRect(q.at(0).toInt(), q.at(1).toInt(),
                                             q.at(2).toInt(), q.at(3).toInt()));
        }
        const QJsonObject colorMap = a.value("nets").toObject();
        for (auto it = colorMap.constBegin(); it != colorMap.constEnd(); ++it)
        {
            // The key is a net name or a numeric id written as text, so a number wins if it parses
            bool isNum = false;
            const int asNum = it.key().toInt(&isNum);
            QString neterr;
            net_t n = resolveNetChecked(isNum ? QJsonValue(asNum) : QJsonValue(it.key()), neterr);
            if (n == 0) return errorResult(neterr);
            const QColor c = QColor::fromString(it.value().toString());
            if (!c.isValid())
                return errorResult(QString("'%1' is not a colour I can parse; use #rrggbb")
                                       .arg(it.value().toString()));
            spec.netColors.insert(n, c);
        }

        RenderResult res = ::controller.getRenderer().renderRegion(spec);
        if (!res.error.isEmpty())
            return errorResult(res.error);

        const QByteArray png = ClassRenderer::encodePng(res.image);

        QJsonObject meta;
        meta["width"]  = res.image.width();
        meta["height"] = res.image.height();
        QJsonArray world;
        world.append(int(res.worldRect.left()));  world.append(int(res.worldRect.top()));
        world.append(int(res.worldRect.width())); world.append(int(res.worldRect.height()));
        meta["world"]     = world;
        meta["scale"]     = qreal(res.image.width()) / qMax(1.0, res.worldRect.width());
        QJsonArray used;
        for (const QString &l : res.layersUsed)
            used.append(l);
        meta["layers"]    = used;
        meta["png_bytes"] = png.size();
        if (clamped)
            meta["clamped_to"] = MCP_RENDER_MAX_SIDE;
        if (res.pullupsSkipped)
            meta["note_pullups"] = "Pull-up symbols were suppressed: they hide themselves when the "
                                   "scale would render them below about five pixels.";
        if (res.netNamesSkipped)
            meta["note_net_names"] = "Net-name labels were suppressed: they need a scale of 1.5 or "
                                     "more.";

        // Delivery. An explicit path, or a PNG past the inline cap, goes to a file: an inline image
        // is base64 inside the JSON reply, so a large one costs far more context than it is worth.
        const QString wantPath = strArg(a, "path");
        const int cap = a.contains("inline_max_bytes")
                      ? intArg(a, "inline_max_bytes", MCP_RENDER_INLINE_MAX)
                      : MCP_RENDER_INLINE_MAX;
        if (wantPath.isEmpty() && (png.size() <= cap))
        {
            meta["inline"] = true;
            return imageResult(png, meta);
        }

        QString path = wantPath;
        if (path.isEmpty())
        {
            const QString dir = renderOutputDir();
            if (!QDir().mkpath(dir))
                return errorResult(QString("Cannot create the render directory %1").arg(dir));
            path = dir + QString("/render-%1.png")
                             .arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmsszzz"));
        }
        // The application sets its working directory to resource/, so a relative path would land
        // somewhere surprising; resolve it and report what was actually written.
        path = QFileInfo(path).absoluteFilePath();
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || (f.write(png) != png.size()))
            return errorResult(QString("Cannot write %1: %2").arg(path, f.errorString()));
        f.close();

        meta["inline"] = false;
        meta["path"]   = QDir::toNativeSeparators(path);
        return imageResult(QByteArray(), meta);
    });
}

QJsonValue ClassMcpTools::hndWatchlistGet(const QJsonObject &, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        ClassWatch &w = ::controller.getWatch();
        const QStringList names = w.getWatchlist();
        QJsonArray arr;
        for (const QString &n : names)
            arr.append(n);
        QJsonObject r;
        r["watchlist"]     = arr;
        r["count"]         = arr.size();
        r["history_depth"] = w.historyDepth();
        // The half-open range that holds data. Anything outside it reads as the no-data sentinel.
        r["hstart"]        = qint64(w.gethstart());
        r["hlast"]         = qint64(w.gethlast());
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
        // Resolve every requested net before touching the watchlist, so a name that exists nowhere
        // fails the whole call with near matches rather than yielding a silently missing column.
        QStringList keys;
        for (const QJsonValue &v : names)
        {
            const QString key = resolveWatchKey(v, err);
            if (key.isEmpty()) return QJsonValue{};
            if (!keys.contains(key))
                keys.append(key);
        }

        ClassWatch &w = ::controller.getWatch();
        // Save prior watchlist
        QStringList prior = w.getWatchlist();
        QStringList merged = prior;
        for (const QString &key : keys)
        {
            if (!merged.contains(key))
                merged.append(key);
        }
        // The M/T label is derived from the state latches rather than recorded in halfCycle(): that
        // loop already pays a name lookup per watched net per half-cycle, and only a caller asking
        // for a window needs the label. The latches join the watchlist for the duration; they reach
        // the reply only when the caller listed them itself, since the sample loop walks `keys`.
        static const char *kLatchNets[] = { "m1", "m2", "m3", "m4", "m5", "m6",
                                            "t1", "t2", "t3", "t4", "t5", "t6" };
        QStringList latches;
        for (const char *ln : kLatchNets)
        {
            const QString name = QString::fromLatin1(ln);
            if (!::controller.getNetlist().get(name))
                continue;               // m6 carries no name today; it was renamed to ixy_d_phase
            latches.append(name);
            if (!merged.contains(name))
                merged.append(name);
        }
        w.updateWatchlist(merged);

        uint hcStart = 0;
        if (doReset)
        {
            // doReset() runs the reset sequence and returns how many half-cycles it burned. Those
            // half-cycles are sampled too, so the window has to start after them or the caller gets
            // the reset propagation prepended to the run it asked for.
            hcStart = ::controller.doReset();
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
        // getCurrentHCycle() is one past the last half-cycle that was written, so the window is
        // half-open. Including hcEnd would read an unwritten slot, where ClassWatch::at returns its
        // no-data sentinel — which is what used to put a trailing 3 on the end of every column.
        uint toHc   = hcEnd;

        QJsonObject samples;
        for (const QString &name : keys)
        {
            watch *wp = w.find(name);
            if (!wp)
            {
                err = QString("Net \"%1\" resolved but could not be watched.").arg(name);
                return QJsonValue{};
            }
            QJsonArray arr;
            for (uint hc = fromHc; hc < toHc; hc++)
                arr.append(int(w.at(wp, hc)));
            samples[name] = arr;
        }

        // Half-cycle index and M/T label per sample, aligned with the columns above.
        watch *mWatch[7] = {};
        watch *tWatch[7] = {};
        for (const QString &ln : latches)
        {
            const int idx = ln.mid(1).toInt();
            if ((idx < 1) || (idx > 6)) continue;
            if (ln.startsWith(QLatin1Char('m')))
                mWatch[idx] = w.find(ln);
            else
                tWatch[idx] = w.find(ln);
        }
        QJsonArray hcArr, mtArr;
        for (uint hc = fromHc; hc < toHc; hc++)
        {
            hcArr.append(qint64(hc));
            QChar mc = QLatin1Char('?');
            QChar tc = QLatin1Char('?');
            for (int i = 1; i <= 6; i++)
            {
                if (mWatch[i] && (w.at(mWatch[i], hc) == 1)) { mc = QLatin1Char('0' + i); break; }
            }
            for (int i = 1; i <= 6; i++)
            {
                if (tWatch[i] && (w.at(tWatch[i], hc) == 1)) { tc = QLatin1Char('0' + i); break; }
            }
            mtArr.append(QString("M%1T%2").arg(mc).arg(tc));
        }

        // Restore prior watchlist
        w.updateWatchlist(prior);

        QJsonObject r;
        r["hc_start"] = qint64(fromHc);
        r["hc_end"]   = qint64(toHc);
        r["hc"]       = hcArr;
        r["mt"]       = mtArr;
        r["samples"]  = samples;
        return structuredResult(r);
    });
}

QJsonValue ClassMcpTools::hndEquationTree(const QJsonObject &a, QString &err)
{
    QJsonArray items;
    bool plural = false;
    if (!batchArgs(a, "net", "nets", MCP_BATCH_CAP_TREE, items, plural, err)) return {};

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        return batchResult(items, plural, &ClassMcpTools::equationTreeObject);
    });
}

QJsonValue ClassMcpTools::hndNetDrivers(const QJsonObject &a, QString &err)
{
    QJsonArray items;
    bool plural = false;
    if (!batchArgs(a, "net", "nets", MCP_BATCH_CAP, items, plural, err)) return {};

    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        return batchResult(items, plural, &ClassMcpTools::netDriversObject);
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

/*
 * Writes the user data files. Any unrecognised id is a mistake the model can correct, so the whole
 * call is rejected with the list of valid ones rather than silently writing the rest: this tool
 * rewrites files in full, and a caller that misspelled one id may have meant something else by the
 * others. An id that is known but has nothing behind it yet, such as a waveform view that is not
 * open, is a real state of the application and is reported per item under "failed".
 */
QJsonValue ClassMcpTools::hndSave(const QJsonObject &a, QString &)
{
    return ClassMcpThreading::callOnMain([&]() -> QJsonValue {
        QStringList known;
        for (const ClassController::SaveItem &item : ::controller.saveItems())
            known.append(item.id);

        // Distinguish "no items key" (save everything) from a malformed or empty one. Defaulting a
        // bad argument to "save everything" would turn a typo into a full rewrite of every file.
        QStringList ids;
        if (a.contains("items"))
        {
            const QJsonValue v = a.value("items");
            if (!v.isArray())
                return errorResult("'items' must be an array of save item ids. Omit it entirely to save everything.");
            const QJsonArray items = v.toArray();
            if (items.isEmpty())
                return errorResult("'items' is empty, so nothing would be saved. Omit it entirely to save everything.");
            for (const QJsonValue &e : items)
            {
                if (!e.isString())
                    return errorResult(QString("Every entry of 'items' must be a string. Valid ids are: %1.").arg(known.join(", ")));
                ids.append(e.toString().trimmed());
            }
            QStringList unknown;
            for (const QString &id : std::as_const(ids))
            {
                if (!known.contains(id))
                    unknown.append(id);
            }
            if (!unknown.isEmpty())
                return errorResult(QString("No such save item: %1. Valid ids are: %2. Omit 'items' to save everything.")
                                   .arg(unknown.join(", "), known.join(", ")));
        }

        QJsonArray saved, skipped, failed;
        const QVector<ClassController::SaveResult> results = ::controller.save(ids);
        for (const ClassController::SaveResult &r : results)
        {
            if (r.outcome == ClassController::SaveWritten)
                saved.append(QJsonObject{{"id", r.id}, {"files", QJsonArray::fromStringList(r.files)}});
            else if (r.outcome == ClassController::SaveSkipped)
                skipped.append(QJsonObject{{"id", r.id}, {"reason", r.reason}});
            else
                failed.append(QJsonObject{{"id", r.id}, {"reason", r.reason}});
        }
        QJsonObject out;
        out["saved"] = saved;
        out["skipped"] = skipped;
        out["failed"] = failed;
        return structuredResult(out);
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
