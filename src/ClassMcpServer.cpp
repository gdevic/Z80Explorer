#include "ClassMcpServer.h"
#include "ClassMcpTools.h"
#include <QDebug>
#include <QHttpHeaders>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrl>

// Protocol constants. This server speaks exactly one MCP revision; see the class note for why.
static const char *MCP_PROTOCOL_VERSION = "2026-07-28";
static const char *MCP_SERVER_NAME      = "z80explorer";
static const char *MCP_SERVER_VERSION   = "2.0.0";

// Reserved _meta keys the stateless revision defines for the per-request protocol fields
static const char *META_PROTOCOL_VERSION    = "io.modelcontextprotocol/protocolVersion";
static const char *META_CLIENT_CAPABILITIES = "io.modelcontextprotocol/clientCapabilities";
static const char *META_SERVER_INFO         = "io.modelcontextprotocol/serverInfo";

// How long a client may cache tools/list. The tool table is built once at startup and never
// changes afterwards, so caching it costs nothing and saves the schemas on every reconnect.
static const int TOOLS_LIST_TTL_MS = 3600000;

// JSON-RPC 2.0 error codes, plus the codes the MCP specification defines for this revision.
// Application-defined codes live outside the JSON-RPC reserved range, as the spec requires.
enum RpcError {
    ParseError     = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams  = -32602,
    InternalError  = -32603,

    HeaderMismatch                  = -32020,
    MissingRequiredClientCapability = -32021,
    UnsupportedProtocolVersion      = -32022,

    ToolBusy = -31000,
};

/*
 * Sent to the client in the server/discover result. The host puts this in the model's context, so
 * it is the one place where a few hundred tokens buy correct tool use for the whole session.
 */
static const char *kInstructions =
    "Z80Explorer simulates the physical Zilog Z80 die at transistor level: 8,881 transistors and\n"
    "3,597 nets. It is not an instruction-level emulator. Every value you read is the real\n"
    "switch-level state of the chip at a point in time.\n"
    "\n"
    "TIME. The clock is counted in half-cycles; one T-state is two half-cycles. z80_run advances a\n"
    "number of half-cycles, or runs until an `until` condition when halfcycles is 0. It returns\n"
    "stopped_by, one of count, pc, cycle, net, timeout or stop. Always read stopped_by before\n"
    "interpreting the state you landed in: on timeout the run was cut short and the state is\n"
    "wherever it happened to be. z80_now is the cheapest way to find out where you are.\n"
    "\n"
    "NETS. Anywhere a net is taken you may pass its name or its numeric id, and the query tools\n"
    "also take a list to answer in one call. A net value is 0 low, 1 high, 2 floating, or 3 when\n"
    "no sample was recorded for that half-cycle; a bus read through a net-shaped call reports 4,\n"
    "and a bus with a floating member reports 4294967295. Search with z80_net_find rather than\n"
    "guessing a name; an unknown name is reported as an error that lists near matches.\n"
    "\n"
    "TOPOLOGY. z80_net_drivers lists the transistors that pull a net, z80_fanout lists what the net\n"
    "gates, and z80_equation_tree returns the logic tree behind it. These describe the physical\n"
    "circuit and do not change as the simulation runs, so they only need reading once.\n"
    "\n"
    "BATCHING. z80_net_info, z80_net_drivers and z80_trans_info take a list (`nets`, or `ids`)\n"
    "in place of the single target and answer up to 256 of them in one call, each row carrying\n"
    "its own error if it could not be resolved. z80_equation and z80_equation_tree take a list\n"
    "too, capped at 16 because each one walks the netlist. Characterising a group of nets is one\n"
    "call, not one call per net.\n"
    "\n"
    "THE DIE. z80_die_info describes the image, the coordinate system every bounding box is\n"
    "expressed in, and the layer names; read it before reasoning about position rather than\n"
    "inferring the conventions. z80_view_render then draws any region of the die as a PNG you\n"
    "can look at, with per-net colouring and the overlays the interactive view offers. Seeing\n"
    "the geometry often settles a question about what connects to what faster than bounding\n"
    "boxes do.\n"
    "\n"
    "WAVEFORMS. A net must be on the watchlist before any history exists for it. Add nets with\n"
    "z80_watchlist_add, then read recorded samples with z80_waveform_window; z80_watchlist_get\n"
    "reports what is already watched and which half-cycles still have data. Always pass an\n"
    "explicit range: the default spans the whole ring buffer and is large.\n"
    "\n"
    "ESCAPE HATCH. z80_eval_js runs arbitrary JavaScript inside the application and can reach far\n"
    "beyond the simulator. Prefer a dedicated tool whenever one exists.\n";

ClassMcpServer::ClassMcpServer(ClassMcpTools *tools, QObject *parent)
    : QObject(parent), m_tools(tools)
{
}

ClassMcpServer::~ClassMcpServer()
{
    stop();
}

const char *ClassMcpServer::protocolVersion()
{
    return MCP_PROTOCOL_VERSION;
}

QJsonObject ClassMcpServer::serverInfo()
{
    QJsonObject info;
    info["name"]    = MCP_SERVER_NAME;
    info["version"] = MCP_SERVER_VERSION;
    return info;
}

// ===========================================================================
// HTTP plumbing
// ===========================================================================

// Maps our integer HTTP status onto the Qt enumerator. Only the statuses this transport emits are
// listed; anything unexpected degrades to 500 rather than silently answering 200.
static QHttpServerResponder::StatusCode statusOf(int http)
{
    using S = QHttpServerResponder::StatusCode;
    switch (http)
    {
    case 200: return S::Ok;
    case 202: return S::Accepted;
    case 400: return S::BadRequest;
    case 403: return S::Forbidden;
    case 404: return S::NotFound;
    case 405: return S::MethodNotAllowed;
    case 409: return S::Conflict;
    case 415: return S::UnsupportedMediaType;
    case 406: return S::NotAcceptable;
    }
    return S::InternalServerError;
}

// Builds the HTTP reply for one JSON-RPC object, tagging it with the routing headers a gateway
// needs to meter and route without parsing the body.
static QHttpServerResponse jsonReply(const QJsonObject &rpc, int http,
                                     const QString &method, const QString &toolName)
{
    QHttpServerResponse resp(QByteArray("application/json"),
                             QJsonDocument(rpc).toJson(QJsonDocument::Compact),
                             statusOf(http));
    QHttpHeaders h = resp.headers();
    if (!method.isEmpty())
        h.append("Mcp-Method", method);
    if (!toolName.isEmpty())
        h.append("Mcp-Name", toolName);
    h.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-store");
    resp.setHeaders(h);
    return resp;
}

// An error reply that carries no JSON-RPC id, used for failures detected before a request could be
// read. The spec explicitly allows an id-less error body on these.
static QHttpServerResponse bareError(int http, int code, const QString &message)
{
    QJsonObject err;
    err["code"]    = code;
    err["message"] = message;
    QJsonObject rpc;
    rpc["jsonrpc"] = "2.0";
    rpc["error"]   = err;
    return jsonReply(rpc, http, QString(), QString());
}

bool ClassMcpServer::start(quint16 port)
{
    if (m_http)
    {
        qWarning() << "MCP server: already started";
        return true;
    }

    m_http = new QHttpServer(this);

    // POST /mcp — every JSON-RPC message, request or notification
    m_http->route("/mcp", QHttpServerRequest::Method::Post,
                  [this](const QHttpServerRequest &req) { return routePost(req); });

    // The stateless revision has no server-initiated stream to open on this endpoint: the tool
    // table is fixed at startup, so there is nothing to notify about. Answer honestly rather than
    // holding open a stream that never carries anything.
    m_http->route("/mcp", QHttpServerRequest::Method::Get | QHttpServerRequest::Method::Delete,
                  [](const QHttpServerRequest &) {
                      return bareError(405, MethodNotFound,
                                       "This endpoint accepts POST only; the server pushes no notifications");
                  });

    // GET /mcp/healthz — liveness probe, and the quickest way to see which revision is spoken
    m_http->route("/mcp/healthz", QHttpServerRequest::Method::Get,
                  [this](const QHttpServerRequest &req) { return routeHealthz(req); });

    // QHttpServer::bind() takes ownership of the QTcpServer on success only; pass no parent to
    // avoid double-ownership with our QObject parent chain.
    m_tcp = new QTcpServer;
    if (!m_tcp->listen(QHostAddress::LocalHost, port))
    {
        qCritical() << "MCP server: failed to listen on port" << port << ":" << m_tcp->errorString();
        delete m_http; m_http = nullptr;
        delete m_tcp;  m_tcp  = nullptr;
        return false;
    }
    m_port = m_tcp->serverPort();

    if (!m_http->bind(m_tcp))
    {
        // Ownership did not transfer, so this is the one place the QTcpServer must be deleted here.
        qCritical() << "MCP server: bind() failed on port" << m_port;
        m_tcp->close();
        delete m_tcp;  m_tcp  = nullptr;
        delete m_http; m_http = nullptr;
        m_port = 0;
        return false;
    }

    qInfo() << "MCP server listening on http://127.0.0.1:" << m_port << "/mcp"
            << "protocol" << MCP_PROTOCOL_VERSION;
    return true;
}

void ClassMcpServer::stop()
{
    if (m_tcp)
        m_tcp->close();
    delete m_http;
    m_http = nullptr;
    // m_tcp is owned by QHttpServer once bind() succeeded; do not delete it directly.
    m_tcp  = nullptr;
    m_port = 0;
}

bool ClassMcpServer::isListening() const
{
    return m_tcp && m_tcp->isListening();
}

/*
 * A native MCP client sends no Origin header. A browser always attaches one to a POST, including a
 * same-origin POST, so requiring the header to name loopback on our own port is what stops a page
 * whose hostname has been rebound to 127.0.0.1 from driving the simulator.
 */
bool ClassMcpServer::originAllowed(const QHttpServerRequest &req, QString &why) const
{
    const QHttpHeaders h = req.headers();
    if (!h.contains(QHttpHeaders::WellKnownHeader::Origin))
        return true;

    const QByteArray raw = h.value(QHttpHeaders::WellKnownHeader::Origin).toByteArray();
    if (raw.isEmpty() || (raw == "null"))
    {
        why = QStringLiteral("opaque origin");
        return false;
    }

    const QUrl u(QString::fromUtf8(raw));
    const QString host = u.host();
    const bool loopback = (host == QLatin1String("localhost")) || (host == QLatin1String("127.0.0.1"))
                       || (host == QLatin1String("::1"));
    const bool scheme = (u.scheme() == QLatin1String("http")) || (u.scheme() == QLatin1String("https"));
    if (!loopback || !scheme || (u.port() != int(m_port)))
    {
        why = QString("origin %1 is not this server").arg(QString::fromUtf8(raw));
        return false;
    }
    return true;
}

/*
 * HTTP POST handler: validate the caller, decode one JSON-RPC message, dispatch it, and answer with
 * the status the outcome calls for. Notifications are acknowledged with 202 and no body.
 */
QHttpServerResponse ClassMcpServer::routePost(const QHttpServerRequest &req)
{
    QString why;
    if (!originAllowed(req, why))
        return bareError(403, InvalidRequest, QString("Forbidden: %1").arg(why));

    const QHttpHeaders h = req.headers();

    // Requiring a JSON content type keeps a browser form post out: application/json is not one of
    // the CORS simple content types, so such a request must clear a preflight this server never
    // answers.
    const QByteArray ctype = h.value(QHttpHeaders::WellKnownHeader::ContentType).toByteArray();
    // Qt 6 does not treat left(-1) as "the whole array", so the no-parameter case is split out.
    const qsizetype semi = ctype.indexOf(';');
    const QByteArray media = ((semi >= 0) ? ctype.left(semi) : ctype).trimmed().toLower();
    if (media != "application/json")
        return bareError(415, InvalidRequest,
                         "Content-Type must be application/json");

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(req.body(), &pe);
    if (pe.error != QJsonParseError::NoError)
        return bareError(400, ParseError, QString("Parse error: %1").arg(pe.errorString()));

    // JSON-RPC batching was removed from the protocol; an array is no longer a valid body.
    if (doc.isArray())
        return bareError(400, InvalidRequest,
                         "JSON-RPC batching is not supported; send one message per request");
    if (!doc.isObject())
        return bareError(400, InvalidRequest, "Request body must be a single JSON-RPC object");

    const QJsonObject request = doc.object();
    const QString method = request.value("method").toString();
    const QString toolName = (method == QLatin1String("tools/call"))
                           ? request.value("params").toObject().value("name").toString()
                           : QString();

    // The standard request headers mirror body fields so that intermediaries can route without
    // parsing. They are checked here rather than before the parse because every check compares a
    // header against the body it is supposed to mirror.
    int http = 200;
    const QJsonObject headerErr = checkRequestHeaders(h, request, method, http);
    if (!headerErr.isEmpty())
        return jsonReply(headerErr, http, method, toolName);

    const QJsonObject reply = dispatch(request, http);
    if (reply.isEmpty())
    {
        // A notification was accepted; the spec requires 202 with no body.
        QHttpServerResponse resp(statusOf(202));
        return resp;
    }
    return jsonReply(reply, http, method, toolName);
}

QHttpServerResponse ClassMcpServer::routeHealthz(const QHttpServerRequest &req)
{
    QString why;
    if (!originAllowed(req, why))
        return bareError(403, InvalidRequest, QString("Forbidden: %1").arg(why));

    QJsonObject o;
    o["status"]            = "ok";
    o["server"]            = MCP_SERVER_NAME;
    o["version"]           = MCP_SERVER_VERSION;
    o["supportedVersions"] = QJsonArray{ MCP_PROTOCOL_VERSION };
    o["tools"]             = m_tools ? m_tools->toolCount() : 0;
    return QHttpServerResponse(QByteArray("application/json"),
                               QJsonDocument(o).toJson(QJsonDocument::Compact));
}

// ===========================================================================
// JSON-RPC dispatch
// ===========================================================================

/*
 * The stateless revision requires every request to declare the protocol version it is speaking and
 * the capabilities the client offers.
 */
QJsonObject ClassMcpServer::checkRequestMeta(const QJsonObject &req, const QString &method, int &httpStatus)
{
    // Discovery and ping are exempt because a client has to be able to ask what this server is
    // before it can know what to declare. initialize is exempt so that a client still using the old
    // handshake reaches the arm that tells it which revision to switch to, rather than being told
    // its request is missing a field the old handshake never had.
    if ((method == QLatin1String("server/discover")) || (method == QLatin1String("ping"))
     || (method == QLatin1String("initialize")))
        return {};

    const QJsonValue id = req.value("id");
    const QJsonObject meta = req.value("params").toObject().value("_meta").toObject();

    const QString ver = meta.value(META_PROTOCOL_VERSION).toString();
    if (ver.isEmpty())
    {
        httpStatus = 400;
        return makeError(id, InvalidParams,
                         QString("Missing required _meta field \"%1\"").arg(META_PROTOCOL_VERSION));
    }
    if (ver != QLatin1String(MCP_PROTOCOL_VERSION))
    {
        httpStatus = 400;
        QJsonObject data;
        data["supportedVersions"] = QJsonArray{ MCP_PROTOCOL_VERSION };
        return makeError(id, UnsupportedProtocolVersion,
                         QString("This server speaks MCP %1 only; the request declared %2")
                             .arg(MCP_PROTOCOL_VERSION, ver), data);
    }
    if (!meta.contains(META_CLIENT_CAPABILITIES))
    {
        httpStatus = 400;
        return makeError(id, InvalidParams,
                         QString("Missing required _meta field \"%1\"").arg(META_CLIENT_CAPABILITIES));
    }
    // MissingRequiredClientCapability (-32021) is declared but deliberately never raised: it says
    // the server needs a capability the client did not offer, and no tool here needs one. The
    // clientCapabilities field is required to be present, but its contents are never consulted.
    return {};
}

QString ClassMcpServer::decodeHeaderValue(const QByteArray &raw)
{
    const QByteArray v = raw.trimmed();
    // The sentinel is exactly "=?base64?" + payload + "?=", so anything shorter than the two
    // markers together cannot be one.
    if (v.startsWith("=?base64?") && v.endsWith("?=") && (v.size() > 11))
    {
        const auto res = QByteArray::fromBase64Encoding(v.mid(9, v.size() - 11));
        if (res)
            return QString::fromUtf8(res.decoded);
        // Not valid base64 after all; fall through and compare the literal text, which will
        // mismatch and be reported as such rather than silently succeeding.
    }
    return QString::fromUtf8(v);
}

/*
 * Validates the headers the transport mirrors from the body: MCP-Protocol-Version, Mcp-Method and
 * Mcp-Name. The specification makes all three REQUIRED and says a server MUST reject a request that
 * omits one. This server deliberately accepts an omitted header and logs a warning instead, so that
 * it interoperates with any client, while still rejecting a header that contradicts the body — the
 * case the rule exists to guard, where an intermediary routes on the header and the server acts on
 * the body. tests/mcp_integration.py pins both halves of that behaviour down.
 */
QJsonObject ClassMcpServer::checkRequestHeaders(const QHttpHeaders &h, const QJsonObject &req,
                                                const QString &method, int &httpStatus)
{
    // One notice per missing header for the life of the process: the deviation is worth stating
    // once, not once per request. Handlers run only on the GUI thread, so plain statics are safe.
    static bool warnedVersion = false;
    static bool warnedMethod  = false;
    static bool warnedName    = false;

    const QJsonValue id = req.value("id");
    const QJsonObject params = req.value("params").toObject();

    if (h.contains("MCP-Protocol-Version"))
    {
        const QString hv = decodeHeaderValue(h.combinedValue("MCP-Protocol-Version"));
        const QString bv = params.value("_meta").toObject().value(META_PROTOCOL_VERSION).toString();
        // A header that disagrees with the body is a mismatch. A header that agrees (or a body that
        // declares nothing, as on the _meta-exempt methods) but names a revision this server does
        // not speak is a version error, which the client can renegotiate from.
        if (!bv.isEmpty() && (hv != bv))
        {
            httpStatus = 400;
            return makeError(id, HeaderMismatch,
                             QString("Header mismatch: MCP-Protocol-Version header value \"%1\" does "
                                     "not match body value \"%2\"").arg(hv, bv));
        }
        if (hv != QLatin1String(MCP_PROTOCOL_VERSION))
        {
            httpStatus = 400;
            QJsonObject data;
            data["supportedVersions"] = QJsonArray{ MCP_PROTOCOL_VERSION };
            return makeError(id, UnsupportedProtocolVersion,
                             QString("Unsupported MCP-Protocol-Version: %1").arg(hv), data);
        }
    }
    else if (!warnedVersion)
    {
        warnedVersion = true;
        qWarning() << "MCP: client omits the required MCP-Protocol-Version header; accepted anyway";
    }

    if (h.contains("Mcp-Method"))
    {
        const QString hm = decodeHeaderValue(h.combinedValue("Mcp-Method"));
        if (hm != method)
        {
            httpStatus = 400;
            return makeError(id, HeaderMismatch,
                             QString("Header mismatch: Mcp-Method header value \"%1\" does not match "
                                     "body value \"%2\"").arg(hm, method));
        }
    }
    else if (!warnedMethod)
    {
        warnedMethod = true;
        qWarning() << "MCP: client omits the required Mcp-Method header; accepted anyway";
    }

    // Mcp-Name mirrors params.name on tools/call and prompts/get, and params.uri on resources/read.
    const bool named = (method == QLatin1String("tools/call"))
                    || (method == QLatin1String("prompts/get"))
                    || (method == QLatin1String("resources/read"));
    if (named)
    {
        const QString bodyName = (method == QLatin1String("resources/read"))
                               ? params.value("uri").toString()
                               : params.value("name").toString();
        if (h.contains("Mcp-Name"))
        {
            const QString hn = decodeHeaderValue(h.combinedValue("Mcp-Name"));
            if (hn != bodyName)
            {
                httpStatus = 400;
                return makeError(id, HeaderMismatch,
                                 QString("Header mismatch: Mcp-Name header value \"%1\" does not "
                                         "match body value \"%2\"").arg(hn, bodyName));
            }
        }
        else if (!warnedName)
        {
            warnedName = true;
            qWarning() << "MCP: client omits the required Mcp-Name header on" << method
                       << "; accepted anyway";
        }
    }
    return {};
}

QJsonObject ClassMcpServer::dispatch(const QJsonObject &request, int &httpStatus)
{
    httpStatus = 200;

    const QString jsonrpc = request.value("jsonrpc").toString();
    const QString method  = request.value("method").toString();
    const QJsonValue id   = request.value("id");
    const bool isNotification = !request.contains("id");

    if (jsonrpc != QLatin1String("2.0"))
    {
        if (isNotification) return {};
        httpStatus = 400;
        return makeError(id, InvalidRequest, "Missing or invalid jsonrpc version (expected \"2.0\")");
    }
    if (method.isEmpty())
    {
        if (isNotification) return {};
        httpStatus = 400;
        return makeError(id, InvalidRequest, "Missing method");
    }

    // Notifications never get a reply. notifications/cancelled is accepted and ignored: a tool call
    // runs to completion on the GUI thread, so there is no in-flight work to abandon.
    if (isNotification)
        return {};

    // The id of a request must not be null in this revision.
    if (id.isNull())
    {
        httpStatus = 400;
        return makeError(QJsonValue(QJsonValue::Null), InvalidRequest, "Request id must not be null");
    }

    const QJsonObject metaErr = checkRequestMeta(request, method, httpStatus);
    if (!metaErr.isEmpty())
        return metaErr;

    if (method == QLatin1String("server/discover")) return handleDiscover(request);
    if (method == QLatin1String("tools/list"))      return handleToolsList(request);
    if (method == QLatin1String("tools/call"))      return handleToolsCall(request, httpStatus);
    if (method == QLatin1String("ping"))            return makeResult(id, QJsonObject());

    if (method == QLatin1String("resources/list"))           return handleResourcesList(request);
    if (method == QLatin1String("resources/templates/list")) return handleResourceTemplates(request);
    if (method == QLatin1String("resources/read"))           return handleResourcesRead(request, httpStatus);
    if (method == QLatin1String("prompts/list"))             return handlePromptsList(request);
    if (method == QLatin1String("prompts/get"))              return handlePromptsGet(request, httpStatus);
    if (method == QLatin1String("completion/complete"))      return handleComplete(request);

    // The handshake this revision removed. Answering with the supported-version error is what lets
    // a client that opened with the old flow recognise the situation and switch.
    if (method == QLatin1String("initialize"))
    {
        httpStatus = 400;
        QJsonObject data;
        data["supportedVersions"] = QJsonArray{ MCP_PROTOCOL_VERSION };
        return makeError(id, UnsupportedProtocolVersion,
                         QString("This server speaks MCP %1, which has no initialize handshake; "
                                 "use server/discover").arg(MCP_PROTOCOL_VERSION), data);
    }

    httpStatus = 404;
    return makeError(id, MethodNotFound, QString("Unknown method: %1").arg(method));
}

QJsonObject ClassMcpServer::capabilities() const
{
    QJsonObject caps;

    // The tool table is built once at startup and never changes, so there is nothing to subscribe
    // to. Saying so lets a client cache the list instead of polling it.
    QJsonObject tools;
    tools["listChanged"] = false;
    caps["tools"] = tools;

    QJsonObject completions;
    caps["completions"] = completions;

    QJsonObject resources;
    resources["listChanged"] = false;
    resources["subscribe"]   = false;
    caps["resources"] = resources;

    QJsonObject prompts;
    prompts["listChanged"] = false;
    caps["prompts"] = prompts;

    return caps;
}

QJsonObject ClassMcpServer::handleDiscover(const QJsonObject &req)
{
    QJsonObject result;
    result["supportedVersions"] = QJsonArray{ MCP_PROTOCOL_VERSION };
    result["capabilities"]      = capabilities();
    result["instructions"]      = kInstructions;
    result["ttlMs"]             = TOOLS_LIST_TTL_MS;
    result["cacheScope"]        = "public";
    return makeResult(req.value("id"), result);
}

QJsonObject ClassMcpServer::handleToolsList(const QJsonObject &req)
{
    QJsonObject result;
    result["tools"]      = m_tools ? m_tools->toolsList() : QJsonArray();
    result["ttlMs"]      = TOOLS_LIST_TTL_MS;
    result["cacheScope"] = "public";
    return makeResult(req.value("id"), result);
}

QJsonObject ClassMcpServer::handleToolsCall(const QJsonObject &req, int &httpStatus)
{
    const QJsonValue id = req.value("id");
    const QJsonObject params = req.value("params").toObject();
    const QString name = params.value("name").toString();
    if (name.isEmpty())
    {
        httpStatus = 400;
        return makeError(id, InvalidParams, "tools/call: missing 'name'");
    }

    const QJsonValue args = params.value("arguments");
    if (!args.isObject() && !args.isUndefined() && !args.isNull())
    {
        httpStatus = 400;
        return makeError(id, InvalidParams, "tools/call: 'arguments' must be an object");
    }

    if (!m_tools)
        return makeError(id, InternalError, "No tool registry bound");

    // An unknown tool is a protocol error, not a tool execution error: the model cannot fix it by
    // adjusting arguments, it has to look at the list again.
    if (!m_tools->hasTool(name))
    {
        httpStatus = 400;
        return makeError(id, InvalidParams,
                         QString("Unknown tool: %1. Call tools/list for the current names.").arg(name));
    }

    // A long tool call spins a nested event loop, so a second request can arrive mid-flight and be
    // dispatched from inside the first one's stack. Reads tolerate that; anything that moves the
    // simulation or edits the watchlist does not, and would corrupt the run already in progress.
    const bool reentrant = m_tools->isReentrant(name);
    if (m_toolBusy && !reentrant)
    {
        httpStatus = 409;
        return makeError(id, ToolBusy,
                         QString("%1 cannot run while another simulation tool is in progress. "
                                 "Call z80_stop first, or retry once the running call returns.").arg(name));
    }

    QString err;
    QJsonValue result;
    {
        const bool guard = !reentrant;
        if (guard) m_toolBusy = true;
        result = m_tools->invoke(name, args.toObject(), err);
        if (guard) m_toolBusy = false;
    }

    if (!err.isEmpty())
    {
        // A genuine handler failure is a tool execution error: the model sees the text and can
        // correct its arguments, which is exactly what isError is for.
        QJsonObject text;
        text["type"] = "text";
        text["text"] = err;
        QJsonObject tr;
        tr["content"] = QJsonArray{ text };
        tr["isError"] = true;
        return makeResult(id, tr);
    }
    return makeResult(id, result);
}

// ===========================================================================
// Resources, prompts and completion
// ===========================================================================

QJsonObject ClassMcpServer::handleResourcesList(const QJsonObject &req)
{
    QJsonObject result;
    result["resources"]  = m_tools ? m_tools->resourcesList() : QJsonArray();
    result["ttlMs"]      = TOOLS_LIST_TTL_MS;
    result["cacheScope"] = "public";
    return makeResult(req.value("id"), result);
}

QJsonObject ClassMcpServer::handleResourceTemplates(const QJsonObject &req)
{
    QJsonObject result;
    result["resourceTemplates"] = m_tools ? m_tools->resourceTemplatesList() : QJsonArray();
    result["ttlMs"]             = TOOLS_LIST_TTL_MS;
    result["cacheScope"]        = "public";
    return makeResult(req.value("id"), result);
}

QJsonObject ClassMcpServer::handleResourcesRead(const QJsonObject &req, int &httpStatus)
{
    const QJsonValue id = req.value("id");
    const QString uri = req.value("params").toObject().value("uri").toString();
    if (uri.isEmpty())
    {
        httpStatus = 400;
        return makeError(id, InvalidParams, "resources/read: missing 'uri'");
    }
    if (!m_tools)
        return makeError(id, InternalError, "No tool registry bound");

    QString err;
    const QJsonValue value = m_tools->readResource(uri, err);
    if (!err.isEmpty())
    {
        // An unreadable URI is a request the model cannot repair by retrying the same way, so it is
        // a protocol error rather than a result carrying isError.
        httpStatus = 400;
        return makeError(id, InvalidParams, err);
    }
    // resources/read is one of the CacheableResult methods, so it carries the freshness hint too.
    QJsonObject result = value.toObject();
    result["ttlMs"]      = TOOLS_LIST_TTL_MS;
    result["cacheScope"] = "public";
    return makeResult(id, result);
}

QJsonObject ClassMcpServer::handlePromptsList(const QJsonObject &req)
{
    QJsonObject result;
    result["prompts"]    = m_tools ? m_tools->promptsList() : QJsonArray();
    result["ttlMs"]      = TOOLS_LIST_TTL_MS;
    result["cacheScope"] = "public";
    return makeResult(req.value("id"), result);
}

QJsonObject ClassMcpServer::handlePromptsGet(const QJsonObject &req, int &httpStatus)
{
    const QJsonValue id = req.value("id");
    const QJsonObject params = req.value("params").toObject();
    const QString name = params.value("name").toString();
    if (name.isEmpty())
    {
        httpStatus = 400;
        return makeError(id, InvalidParams, "prompts/get: missing 'name'");
    }
    if (!m_tools)
        return makeError(id, InternalError, "No tool registry bound");

    QString err;
    const QJsonValue result = m_tools->getPrompt(name, params.value("arguments").toObject(), err);
    if (!err.isEmpty())
    {
        httpStatus = 400;
        return makeError(id, InvalidParams, err);
    }
    return makeResult(id, result);
}

QJsonObject ClassMcpServer::handleComplete(const QJsonObject &req)
{
    const QJsonObject params = req.value("params").toObject();
    const QJsonObject argument = params.value("argument").toObject();
    QJsonObject result;
    if (m_tools)
        result = m_tools->complete(params.value("ref").toObject(),
                                   argument.value("name").toString(),
                                   argument.value("value").toString());
    return makeResult(req.value("id"), result);
}

// ===========================================================================
// Envelope builders
// ===========================================================================

QJsonObject ClassMcpServer::makeError(const QJsonValue &id, int code, const QString &message,
                                      const QJsonValue &data)
{
    QJsonObject err;
    err["code"]    = code;
    err["message"] = message;
    if (!data.isUndefined())
        err["data"] = data;
    QJsonObject r;
    r["jsonrpc"]   = "2.0";
    r["id"]        = id.isUndefined() ? QJsonValue(QJsonValue::Null) : id;
    r["error"]     = err;
    return r;
}

/*
 * Every result funnels through here, which is where the two fields this revision expects on all of
 * them are added: resultType, and the server identity in _meta. Handlers that already set either
 * one keep their value, so a future input_required result needs no change here.
 */
QJsonObject ClassMcpServer::makeResult(const QJsonValue &id, const QJsonValue &result)
{
    QJsonValue res = result;
    if (res.isObject())
    {
        QJsonObject o = res.toObject();
        if (!o.contains("resultType"))
            o["resultType"] = "complete";

        QJsonObject meta = o.value("_meta").toObject();
        if (!meta.contains(META_SERVER_INFO))
            meta[META_SERVER_INFO] = serverInfo();
        o["_meta"] = meta;

        res = o;
    }

    QJsonObject r;
    r["jsonrpc"]   = "2.0";
    r["id"]        = id.isUndefined() ? QJsonValue(QJsonValue::Null) : id;
    r["result"]    = res;
    return r;
}
