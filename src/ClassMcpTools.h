#ifndef CLASSMCPTOOLS_H
#define CLASSMCPTOOLS_H

#include "AppTypes.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

/*
 * ClassMcpTools — MCP tool registry and dispatcher.
 *
 * Holds a table of ToolDef { name, description, inputSchema, handler }
 * entries. registerDefaults() populates the 26 built-in tools covering
 * execution control, state reads, memory/IO, pin control, breakpoints,
 * spatial queries, rendering, and an eval_js escape hatch.
 *
 * invoke(name, args, err) dispatches to the matching handler. Each
 * handler marshals to the main thread via ClassMcpThreading::callOnMain()
 * before touching controller state or the Qt graphics stack. Handlers
 * must not throw — they report errors by filling `err` and returning a
 * null QJsonValue.
 *
 * Handlers return the MCP tool-call result format:
 *   { "content": [ {"type":"text","text":"..."} ], "isError": false }
 * Helper factories below build these shapes.
 */
class ClassMcpTools : public QObject
{
    Q_OBJECT
public:
    // A handler takes the arguments object and may set err on failure.
    // Return value is the full tool-call result object (with "content").
    using Handler = std::function<QJsonValue(const QJsonObject &args, QString &err)>;

    struct ToolDef
    {
        QString name;
        QString description;
        QJsonObject inputSchema;        // JSON Schema for the arguments
        Handler handler;
    };

    explicit ClassMcpTools(QObject *parent = nullptr);

    void registerDefaults();            // Populate the 26 built-in tools
    void registerTool(const ToolDef &t);// Add a custom tool (for tests)

    QJsonArray toolsList() const;       // For MCP tools/list response
    QJsonValue invoke(const QString &name, const QJsonObject &args, QString &err);
    int toolCount() const { return m_tools.size(); }
    const QVector<ToolDef> &tools() const { return m_tools; }

    // Result builders (public so tests and custom tools can use them)
    static QJsonValue textResult(const QString &text, bool isError = false);
    static QJsonValue textResult(const QJsonValue &jsonPayload);

private:
    // Common argument-parsing helpers
    static int    intArg(const QJsonObject &a, const QString &key, int def = 0);
    static uint   uintArg(const QJsonObject &a, const QString &key, uint def = 0);
    static bool   boolArg(const QJsonObject &a, const QString &key, bool def = false);
    static QString strArg(const QJsonObject &a, const QString &key, const QString &def = {});

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
    QJsonValue hndFanout        (const QJsonObject &a, QString &err);
    QJsonValue hndWatchlistAdd  (const QJsonObject &a, QString &err);
    QJsonValue hndSampleWindow  (const QJsonObject &a, QString &err);

    // Structured-equation + direct-driver helpers.
    QJsonValue hndEquationTree  (const QJsonObject &a, QString &err);
    QJsonValue hndNetDrivers    (const QJsonObject &a, QString &err);

    // Resolve a "net" argument that may be either a string name or an integer id.
    net_t resolveNet(const QJsonValue &v) const;
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
