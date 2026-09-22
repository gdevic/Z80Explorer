#ifndef CLASSMCPTOOLS_H
#define CLASSMCPTOOLS_H

#include "AppTypes.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

/*
 * ClassMcpTools — MCP tool registry and dispatcher.
 *
 * Holds a table of ToolDef entries carrying a name, description, title,
 * input and output schemas, behaviour hints and the handler.
 * registerDefaults() populates the built-in tools covering execution
 * control, state reads, memory and IO, pin control, breakpoints,
 * topology queries, waveform capture, and an eval_js escape hatch;
 * applyToolMetadata() then stamps the per-tool metadata from one table.
 *
 * invoke(name, args, err) dispatches to the matching handler. Each
 * handler marshals to the main thread via ClassMcpThreading::callOnMain()
 * before touching controller state or the Qt graphics stack. Handlers
 * must not throw — they report errors by filling `err` and returning a
 * null QJsonValue, or by returning errorResult() for a failure the model
 * can correct by retrying with different arguments.
 *
 * Handlers return the MCP tool-call result format. Prefer
 * structuredResult(), which emits the payload as structuredContent for
 * the client to validate against outputSchema and, for text-only clients,
 * the same JSON in a text block.
 */
class ClassMcpTools : public QObject
{
    Q_OBJECT
public:
    // A handler takes the arguments object and may set err on failure.
    // Return value is the full tool-call result object (with "content").
    using Handler = std::function<QJsonValue(const QJsonObject &args, QString &err)>;

    // Behaviour hints published with each tool. They tell a client how much damage a call can do,
    // which is what lets a host auto-approve the pure reads instead of prompting for all of them.
    struct ToolHints
    {
        bool readOnly    {false};       // Does not modify any state
        bool destructive {false};       // May discard or overwrite state the user cares about
        bool idempotent  {true};        // Repeating the call with the same arguments changes nothing
        bool openWorld   {false};       // Reaches outside the simulator (filesystem, processes)
    };

    struct ToolDef
    {
        QString name;
        QString description;
        QJsonObject inputSchema;        // JSON Schema for the arguments
        Handler handler;
        QString title;                  // Human-readable name for display
        QJsonObject outputSchema;       // JSON Schema for structuredContent; empty when unstructured
        ToolHints hints;
        bool reentrant {true};          // Safe to run while another tool holds a nested event loop
    };

    explicit ClassMcpTools(QObject *parent = nullptr);

    void registerDefaults();            // Populate the built-in tools
    void registerTool(const ToolDef &t);// Add a custom tool (for tests)

    QJsonArray toolsList() const;       // For MCP tools/list response
    QJsonValue invoke(const QString &name, const QJsonObject &args, QString &err);

    // Resources. The chip's topology never changes while the app runs, so the bulk of it is better
    // fetched once as a resource than re-queried through a tool on every question about it.
    QJsonArray resourcesList() const;
    QJsonArray resourceTemplatesList() const;
    QJsonValue readResource(const QString &uri, QString &err);

    // Prompts: the investigations this project repeats, so they do not have to be re-specified.
    QJsonArray promptsList() const;
    QJsonValue getPrompt(const QString &name, const QJsonObject &args, QString &err);

    // Argument completion. Net names are the argument a model gets wrong most often, and the spec
    // scopes completion to prompt arguments and resource-template variables, which is where the
    // net-name variables live.
    QJsonObject complete(const QJsonObject &ref, const QString &argName, const QString &value) const;
    int toolCount() const { return m_tools.size(); }
    const QVector<ToolDef> &tools() const { return m_tools; }
    bool hasTool(const QString &name) const;
    bool isReentrant(const QString &name) const;

    // Result builders (public so tests and custom tools can use them)
    static QJsonValue textResult(const QString &text, bool isError = false);
    static QJsonValue textResult(const QJsonValue &jsonPayload);
    // Preferred builder: emits the payload as validated structuredContent and, for clients that
    // only render text, the same JSON serialized into a text block.
    static QJsonValue structuredResult(const QJsonValue &jsonPayload);

    // A result carrying a PNG the caller can actually look at, plus the metadata that describes
    // it. The content array always holds the text block, because a tools/call result is required
    // to have one; the image block is added only when the PNG is small enough to inline.
    static QJsonValue imageResult(const QByteArray &pngBytes, const QJsonValue &jsonPayload);
    // A tool execution error. The model sees the text and can correct its arguments from it, so the
    // message should say what was wrong and what to do instead.
    static QJsonValue errorResult(const QString &message);

private:
    // Stamps title, behaviour hints, re-entrancy and outputSchema onto the tools registered by
    // registerDefaults(). Kept as a table next to the schemas so the metadata for all tools can be
    // read and audited in one place instead of being scattered across 30-odd registration blocks.
    void applyToolMetadata();

    // Common argument-parsing helpers
    static int    intArg(const QJsonObject &a, const QString &key, int def = 0);
    static uint   uintArg(const QJsonObject &a, const QString &key, uint def = 0);
    static bool   boolArg(const QJsonObject &a, const QString &key, bool def = false);
    static QString strArg(const QJsonObject &a, const QString &key, const QString &def = {});

    // Resolves the scalar-or-list argument convention the query tools share: `one` names the single
    // target, `many` the list form. Exactly one of the two must be present. On success `items` holds
    // the targets and `plural` says which reply shape the caller asked for. Returns false and sets
    // err when the arguments are unusable, including a list longer than cap.
    static bool batchArgs(const QJsonObject &a, const char *one, const char *many, int cap,
                          QJsonArray &items, bool &plural, QString &err);

    // Per-target builders behind the batched query tools. Each returns the same object the scalar
    // form of its tool returns, or an object carrying only `error` when the target cannot be
    // resolved — so one bad entry in a list does not cost the caller the whole batch.
    QJsonObject netInfoObject(const QJsonValue &ref);
    QJsonObject netDriversObject(const QJsonValue &ref);
    QJsonObject transInfoObject(const QJsonValue &ref);
    QJsonObject equationObject(const QJsonValue &ref);
    QJsonObject equationTreeObject(const QJsonValue &ref);

    // Wraps a per-target builder in the scalar or list reply shape chosen by batchArgs().
    QJsonValue batchResult(const QJsonArray &items, bool plural, QJsonObject (ClassMcpTools::*build)(const QJsonValue &));

    // Tool handlers — each returns a tool-call result object.
    QJsonValue hndLoadHex     (const QJsonObject &a, QString &err);
    QJsonValue hndReset       (const QJsonObject &a, QString &err);
    QJsonValue hndRun         (const QJsonObject &a, QString &err);
    QJsonValue hndStop        (const QJsonObject &a, QString &err);
    QJsonValue hndNow         (const QJsonObject &a, QString &err);

    QJsonValue hndNetRead     (const QJsonObject &a, QString &err);
    QJsonValue hndBusRead     (const QJsonObject &a, QString &err);
    QJsonValue hndRegisterRead(const QJsonObject &a, QString &err);
    QJsonValue hndTransRead   (const QJsonObject &a, QString &err);
    QJsonValue hndWaveformWindow(const QJsonObject &a, QString &err);

    QJsonValue hndNetFind     (const QJsonObject &a, QString &err);
    QJsonValue hndNetInfo     (const QJsonObject &a, QString &err);
    QJsonValue hndTransInfo   (const QJsonObject &a, QString &err);
    QJsonValue hndEquation    (const QJsonObject &a, QString &err);

    QJsonValue hndMemRead     (const QJsonObject &a, QString &err);
    QJsonValue hndMemWrite    (const QJsonObject &a, QString &err);
    QJsonValue hndIoRead      (const QJsonObject &a, QString &err);
    QJsonValue hndIoWrite     (const QJsonObject &a, QString &err);

    QJsonValue hndPinSet      (const QJsonObject &a, QString &err);
    QJsonValue hndPinSetAt    (const QJsonObject &a, QString &err);
    QJsonValue hndPinSetAtPc  (const QJsonObject &a, QString &err);

    QJsonValue hndBreakAdd    (const QJsonObject &a, QString &err);
    QJsonValue hndBreakClear  (const QJsonObject &a, QString &err);

    QJsonValue hndViewSet     (const QJsonObject &a, QString &err);
    QJsonValue hndViewGrab    (const QJsonObject &a, QString &err);

    QJsonValue hndEvalJs      (const QJsonObject &a, QString &err);

    // Topology + waveform helpers added for the ALU-flag investigation.
    QJsonValue hndFanout      (const QJsonObject &a, QString &err);
    QJsonValue hndWatchlistAdd(const QJsonObject &a, QString &err);
    QJsonValue hndWatchlistGet(const QJsonObject &a, QString &err);
    QJsonValue hndDieInfo     (const QJsonObject &a, QString &err);
    QJsonValue hndViewRender  (const QJsonObject &a, QString &err);
    QJsonValue hndSampleWindow(const QJsonObject &a, QString &err);

    // Structured-equation + direct-driver helpers.
    QJsonValue hndEquationTree(const QJsonObject &a, QString &err);
    QJsonValue hndNetDrivers  (const QJsonObject &a, QString &err);

    // Net-name management (rename / delete; first-time naming goes through z80_eval_js setNetName).
    QJsonValue hndRenameNet   (const QJsonObject &a, QString &err);
    QJsonValue hndDeleteNetName(const QJsonObject &a, QString &err);

    // Resolve a "net" argument that may be either a string name or an integer id.
    net_t resolveNet(const QJsonValue &v) const;
    // Same, but range-checked, and on failure fills `err` with a message naming the rejected value
    // and, for a misspelled name, the closest existing names. Returns 0 on failure.
    net_t resolveNetChecked(const QJsonValue &v, QString &err) const;
    // Up to `limit` existing net names closest to `name`, used to make a rejection recoverable.
    QStringList nearestNetNames(const QString &name, int limit = 5) const;
    // Resolve one entry of a watch-based tool's `nets` array to the key ClassWatch stores. Returns
    // an empty string and fills `err` when the entry names nothing, so a typo is reported rather
    // than silently dropped from the capture.
    QString resolveWatchKey(const QJsonValue &v, QString &err) const;
    // Resolve the appropriate "current" sim for bit reads (respects USE_AVX2_SIM).
    pin_t readBitByName(const QString &name) const;
    pin_t readBitByNum(net_t n) const;

    QVector<ToolDef> m_tools;

    // Breakpoint registry (simple id → description map for break_clear bookkeeping)
    struct ActiveBreakpoint
    {
        int id {};
        QString kind;                   // "pc" | "cycle" | "net"
        int   arg1 {};                  // pc / cycle / net id
        int   arg2 {};                  // value (for net)
    };
    QVector<ActiveBreakpoint> m_breakpoints;
    int m_nextBreakpointId {1};
};

#endif // CLASSMCPTOOLS_H
