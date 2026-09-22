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
 * ClassMcpServer — MCP (Model Context Protocol) transport layer, revision 2026-07-28.
 *
 * Serves MCP over the Streamable HTTP transport using Qt's QHttpServer: POST /mcp carries every
 * JSON-RPC 2.0 message, GET /mcp/healthz is a liveness probe. Binds to loopback only by design —
 * the simulator is a developer tool, not a network service.
 *
 * 2026-07-28 is a stateless revision: there is no initialize handshake and no session id. Every
 * request carries its own protocol version and client capabilities in the params `_meta` object,
 * and the server answers each request independently. A client discovers what this server offers
 * with `server/discover`, which also returns the `instructions` string that tells a model how to
 * drive the simulator. Only this one revision is spoken; a request naming any other version is
 * answered with UnsupportedProtocolVersion so the client renegotiates rather than guesses.
 *
 * JSON-RPC dispatch routes:
 *   - server/discover : supported versions, capabilities and usage instructions
 *   - tools/list      : enumerate registered MCP tools and their schemas
 *   - tools/call      : invoke a tool with the given arguments
 *   - ping            : liveness
 *
 * Threading: QHttpServer dispatches its route handlers on the thread it was created on, which is
 * the GUI thread, so handlers already run where the controller lives. ClassMcpThreading::callOnMain
 * is kept as a guard for that invariant rather than as a claim that handlers arrive elsewhere.
 * Because a long tool call spins a nested event loop, further requests can be delivered while one
 * is still running; m_toolBusy serialises the tools that cannot tolerate that.
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

    // Handle one decoded JSON-RPC message and return the reply object. `httpStatus` receives the
    // HTTP status the transport answers with, which the spec ties to the error class.
    // If the caller passes a notification (no "id"), returns QJsonObject() (empty).
    QJsonObject dispatch(const QJsonObject &request, int &httpStatus);

    static const char *protocolVersion();   // The single revision this server speaks
    static QJsonObject serverInfo();        // { name, version }, placed in every result's _meta

private:
    QJsonObject handleDiscover(const QJsonObject &req);
    QJsonObject handleToolsList(const QJsonObject &req);
    QJsonObject handleToolsCall(const QJsonObject &req, int &httpStatus);
    QJsonObject handleResourcesList(const QJsonObject &req);
    QJsonObject handleResourceTemplates(const QJsonObject &req);
    QJsonObject handleResourcesRead(const QJsonObject &req, int &httpStatus);
    QJsonObject handlePromptsList(const QJsonObject &req);
    QJsonObject handlePromptsGet(const QJsonObject &req, int &httpStatus);
    QJsonObject handleComplete(const QJsonObject &req);

    QJsonObject capabilities() const;

    // Validates the per-request `_meta` protocol fields the stateless revision requires. Returns an
    // empty object when the request is acceptable, otherwise the JSON-RPC error to send back.
    QJsonObject checkRequestMeta(const QJsonObject &req, const QString &method, int &httpStatus);

    static QJsonObject makeError(const QJsonValue &id, int code, const QString &message,
                                 const QJsonValue &data = QJsonValue(QJsonValue::Undefined));
    static QJsonObject makeResult(const QJsonValue &id, const QJsonValue &result);

    // Rejects cross-origin callers. A native client sends no Origin; a browser always sends one on
    // a POST, so this allowlist is what closes the DNS-rebinding path to the eval_js tool.
    bool originAllowed(const QHttpServerRequest &req, QString &why) const;

    QHttpServerResponse routePost(const QHttpServerRequest &req);
    QHttpServerResponse routeHealthz(const QHttpServerRequest &req);

    ClassMcpTools *m_tools {nullptr};
    QHttpServer   *m_http  {nullptr};
    QTcpServer    *m_tcp   {nullptr};
    quint16        m_port  {};
    bool           m_toolBusy {false};  // Set while a non-reentrant tool runs (see the class note)
};

#endif // CLASSMCPSERVER_H
