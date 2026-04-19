#ifndef CLASSMCPSERVER_H
#define CLASSMCPSERVER_H

#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>

class ClassMcpTools;

/*
 * ClassMcpServer — MCP (Model Context Protocol) transport layer.
 *
 * Serves MCP requests over HTTP, using Qt 6.10's QHttpServer. Follows
 * the Streamable HTTP transport per the MCP spec: POST /mcp for
 * JSON-RPC 2.0 requests, GET /mcp for the server-sent event stream
 * (notifications; we do not push any yet), GET /mcp/healthz for
 * liveness. Binds to localhost only by design — the simulator is a
 * developer tool, not a network service.
 *
 * JSON-RPC dispatch routes three methods:
 *   - initialize     : capability negotiation
 *   - tools/list     : enumerate registered MCP tools and schemas
 *   - tools/call     : invoke a tool with the given arguments
 *
 * All tool invocations are forwarded to ClassMcpTools, which in turn
 * marshals to the main thread via ClassMcpThreading::callOnMain().
 */
class ClassMcpServer : public QObject
{
    Q_OBJECT
public:
    explicit ClassMcpServer(ClassMcpTools *tools, QObject *parent = nullptr);
    ~ClassMcpServer();

    bool start(quint16 port);           // Begin listening. Returns true on success.
    void stop();                        // Stop accepting connections.
    bool isListening() const;
    quint16 port() const { return m_port; }

    // Exposed for tests: handle one decoded JSON-RPC message and return the reply object.
    // If the caller passes a notification (no "id"), returns QJsonObject() (empty).
    QJsonObject dispatch(const QJsonObject &request);

private:
    QJsonObject handleInitialize(const QJsonObject &req);
    QJsonObject handleToolsList(const QJsonObject &req);
    QJsonObject handleToolsCall(const QJsonObject &req);

    static QJsonObject makeError(const QJsonValue &id, int code, const QString &message);
    static QJsonObject makeResult(const QJsonValue &id, const QJsonValue &result);

    QHttpServerResponse routePost(const QHttpServerRequest &req);
    QHttpServerResponse routeHealthz();

    ClassMcpTools *m_tools {nullptr};
    QHttpServer   *m_http  {nullptr};
    QTcpServer    *m_tcp   {nullptr};
    quint16        m_port  {};
};

#endif // CLASSMCPSERVER_H
