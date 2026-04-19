#include "ClassMcpServer.h"
#include "ClassMcpTools.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

// Protocol constants
static const char *MCP_PROTOCOL_VERSION = "2025-11-25";
static const char *MCP_SERVER_NAME      = "z80explorer";
static const char *MCP_SERVER_VERSION   = "1.0.0";

// Standard JSON-RPC 2.0 error codes
enum RpcError {
    ParseError     = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams  = -32602,
    InternalError  = -32603,
};

ClassMcpServer::ClassMcpServer(ClassMcpTools *tools, QObject *parent)
    : QObject(parent), m_tools(tools)
{
}

ClassMcpServer::~ClassMcpServer()
{
    stop();
}

bool ClassMcpServer::start(quint16 port)
{
    if (m_http)
    {
        qWarning() << "MCP server: already started";
        return true;
    }

    m_http = new QHttpServer(this);

    // POST /mcp — JSON-RPC requests
    m_http->route("/mcp", QHttpServerRequest::Method::Post,
                  [this](const QHttpServerRequest &req) { return routePost(req); });

    // GET /mcp — SSE stream placeholder (we don't push notifications yet;
    // returning an empty keep-alive is enough to satisfy clients that open it)
    m_http->route("/mcp", QHttpServerRequest::Method::Get,
                  [](const QHttpServerRequest &) {
                      QHttpServerResponse resp(QByteArray("text/event-stream"),
                                               QByteArray(": ready\n\n"));
                      return resp;
                  });

    // GET /mcp/healthz — liveness probe
    m_http->route("/mcp/healthz", QHttpServerRequest::Method::Get,
                  [this](const QHttpServerRequest &) { return routeHealthz(); });

    // QHttpServer::bind() takes ownership of the QTcpServer; pass no parent
    // to avoid double-ownership with our QObject parent chain.
    m_tcp = new QTcpServer;
    if (!m_tcp->listen(QHostAddress::LocalHost, port))
    {
        qCritical() << "MCP server: failed to listen on port" << port << ":" << m_tcp->errorString();
        delete m_http; m_http = nullptr;
        delete m_tcp;  m_tcp  = nullptr;
        return false;
    }
    m_port = m_tcp->serverPort();
    m_http->bind(m_tcp);

    qInfo() << "MCP server listening on http://localhost:" << m_port << "/mcp";
    return true;
}

void ClassMcpServer::stop()
{
    if (m_tcp)
    {
        m_tcp->close();
    }
    delete m_http;
    m_http = nullptr;
    // m_tcp is owned by QHttpServer after bind(); do not delete directly.
    m_tcp  = nullptr;
    m_port = 0;
}

bool ClassMcpServer::isListening() const
{
    return m_tcp && m_tcp->isListening();
}

/*
 * HTTP POST handler: decode a JSON-RPC request, dispatch it, return the
 * JSON reply. Accepts both single requests and batches (JSON array).
 */
QHttpServerResponse ClassMcpServer::routePost(const QHttpServerRequest &req)
{
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        QJsonObject err = makeError({}, ParseError, QString("Parse error: %1").arg(pe.errorString()));
        return QHttpServerResponse(QByteArray("application/json"),
                                   QJsonDocument(err).toJson(QJsonDocument::Compact));
    }

    QByteArray body;
    if (doc.isArray())
    {
        QJsonArray in = doc.array();
        QJsonArray out;
        for (const QJsonValue &v : in)
        {
            if (!v.isObject()) continue;
            QJsonObject r = dispatch(v.toObject());
            if (!r.isEmpty()) out.append(r);
        }
        body = QJsonDocument(out).toJson(QJsonDocument::Compact);
    }
    else if (doc.isObject())
    {
        QJsonObject r = dispatch(doc.object());
        body = r.isEmpty()
            ? QByteArray() // notification: no reply body
            : QJsonDocument(r).toJson(QJsonDocument::Compact);
    }
    else
    {
        QJsonObject err = makeError({}, InvalidRequest, "Request must be a JSON object or array");
        body = QJsonDocument(err).toJson(QJsonDocument::Compact);
    }

    return QHttpServerResponse(QByteArray("application/json"), body);
}

QHttpServerResponse ClassMcpServer::routeHealthz()
{
    QJsonObject o;
    o["status"]           = "ok";
    o["server"]           = MCP_SERVER_NAME;
    o["version"]          = MCP_SERVER_VERSION;
    o["protocolVersion"]  = MCP_PROTOCOL_VERSION;
    o["tools"]            = m_tools ? m_tools->toolCount() : 0;
    return QHttpServerResponse(QByteArray("application/json"),
                               QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QJsonObject ClassMcpServer::dispatch(const QJsonObject &request)
{
    // JSON-RPC 2.0: must have "jsonrpc":"2.0". If missing "id", it is a notification (no reply).
    const QString jsonrpc = request.value("jsonrpc").toString();
    const QString method  = request.value("method").toString();
    const QJsonValue id   = request.value("id");
    const bool isNotification = !request.contains("id");

    if (jsonrpc != "2.0")
    {
        if (isNotification) return {};
        return makeError(id, InvalidRequest, "Missing or invalid jsonrpc version (expected \"2.0\")");
    }
    if (method.isEmpty())
    {
        if (isNotification) return {};
        return makeError(id, InvalidRequest, "Missing method");
    }

    if (isNotification)
        return {};

    if (method == "initialize")  return handleInitialize(request);
    if (method == "tools/list")  return handleToolsList(request);
    if (method == "tools/call")  return handleToolsCall(request);
    if (method == "ping")        return makeResult(id, QJsonObject());

    return makeError(id, MethodNotFound, QString("Unknown method: %1").arg(method));
}

QJsonObject ClassMcpServer::handleInitialize(const QJsonObject &req)
{
    const QJsonValue id = req.value("id");
    QJsonObject caps;
    caps["tools"] = QJsonObject{};

    QJsonObject info;
    info["name"]    = MCP_SERVER_NAME;
    info["version"] = MCP_SERVER_VERSION;

    QJsonObject result;
    result["protocolVersion"] = MCP_PROTOCOL_VERSION;
    result["capabilities"]    = caps;
    result["serverInfo"]      = info;
    return makeResult(id, result);
}

QJsonObject ClassMcpServer::handleToolsList(const QJsonObject &req)
{
    const QJsonValue id = req.value("id");
    QJsonObject result;
    result["tools"] = m_tools ? m_tools->toolsList() : QJsonArray();
    return makeResult(id, result);
}

QJsonObject ClassMcpServer::handleToolsCall(const QJsonObject &req)
{
    const QJsonValue id = req.value("id");
    QJsonObject params = req.value("params").toObject();
    const QString name = params.value("name").toString();
    if (name.isEmpty())
        return makeError(id, InvalidParams, "tools/call: missing 'name'");

    QJsonValue args = params.value("arguments");
    if (!args.isObject() && !args.isUndefined() && !args.isNull())
        return makeError(id, InvalidParams, "tools/call: 'arguments' must be an object");

    if (!m_tools)
        return makeError(id, InternalError, "No tool registry bound");

    QString err;
    QJsonValue result = m_tools->invoke(name, args.toObject(), err);
    if (!err.isEmpty())
    {
        // Wrap error as an MCP tool error (content + isError=true) so the
        // client sees it as a tool-level failure, not a protocol error.
        QJsonObject tr;
        QJsonArray content;
        QJsonObject text;
        text["type"] = "text";
        text["text"] = err;
        content.append(text);
        tr["content"] = content;
        tr["isError"] = true;
        return makeResult(id, tr);
    }
    return makeResult(id, result);
}

QJsonObject ClassMcpServer::makeError(const QJsonValue &id, int code, const QString &message)
{
    QJsonObject err;
    err["code"]    = code;
    err["message"] = message;
    QJsonObject r;
    r["jsonrpc"]   = "2.0";
    r["id"]        = id.isUndefined() ? QJsonValue(QJsonValue::Null) : id;
    r["error"]     = err;
    return r;
}

QJsonObject ClassMcpServer::makeResult(const QJsonValue &id, const QJsonValue &result)
{
    QJsonObject r;
    r["jsonrpc"]   = "2.0";
    r["id"]        = id.isUndefined() ? QJsonValue(QJsonValue::Null) : id;
    r["result"]    = result;
    return r;
}
