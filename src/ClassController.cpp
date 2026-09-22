#include "ClassController.h"
#include "ClassMcpServer.h"
#include "ClassMcpTools.h"
#include "DialogEditSchematic.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QStringBuilder>

bool ClassController::init(QJSEngine *sc)
{
    qInfo() << "App init...";

    sc->globalObject().setProperty("script", sc->newQObject(&m_script));
    sc->globalObject().setProperty("mon", sc->newQObject(&m_trick));

    // Java engine should not garbage collect or otherwise destruct these objects
    sc->setObjectOwnership(&m_script, QJSEngine::CppOwnership);
    sc->setObjectOwnership(&m_trick, QJSEngine::CppOwnership);

    m_script.init(sc);

    // shutdown() means only "the application is quitting": it stops the simulation and the script
    // engine. Persisting user data is a separate concern, driven by save(), so that it can also be
    // done at any point in a session without tearing the session down.
    connect(this, &ClassController::shutdown, &m_script, &ClassScript::stop);
    connect(this, &ClassController::shutdown, &m_simz80, &ClassSimZ80::onShutdown);
#if USE_AVX2_SIM
    connect(this, &ClassController::shutdown, &m_simz80avx2, &ClassSimZ80_AVX2::onShutdown);
#endif

    QSettings settings;
    // Anchor the default resource directory on the executable's location, not the user's cwd. On macOS
    // applicationDirPath() is .../Z80Explorer.app/Contents/MacOS, so step up to the bundle's Resources
    // dir; on Windows and Linux the binary and the resource folder live side by side at the install root.
#ifdef Q_OS_MACOS
    QString defaultResDir = QCoreApplication::applicationDirPath() + "/../Resources/resource";
#else
    QString defaultResDir = QCoreApplication::applicationDirPath() + "/resource";
#endif
    QString resDir = settings.value("ResourceDir", QDir::cleanPath(defaultResDir)).toString();

#if HAVE_PREBUILT_LAYERMAP
    // Check if the current resource path contains required resource(s)
    qInfo() << "Checking for resource/chip/layermap.bin";
    while (!QFile::exists(resDir + "/chip/layermap.bin") && !QFile::exists(resDir + "/chip/layermap.qz"))
    {
        // Prompts the user to select the chip resource folder. The picker accepts the layermap file inside
        // resource/chip/; the chosen file's grandparent is taken as the resource root.
        QString fileName = QFileDialog::getOpenFileName(nullptr,
        "Select chip/layermap.bin or chip/layermap.qz inside the application resource folder", "layermap.*", "Any file (*.*)");
        if (!fileName.isEmpty())
            resDir = QFileInfo(QFileInfo(fileName).path()).path();
        else
            return false;
    }
    if (!QFile::exists(resDir + "/chip/layermap.bin"))
    {
        // Attempt to uncompress it proceeding to load it
        if (!::controller.uncompressFile(resDir + "/chip/layermap.qz", resDir + "/chip/layermap.bin"))
            return false;
    }
    settings.setValue("ResourceDir", resDir);
#endif
    QDir::setCurrent(resDir);

    // QSettings migration: stale absolute paths from before the resource/ subfolder split. If the stored
    // value still points at the old default-shaped location and no file is there, but the matching file
    // does exist under the new user/ subfolder, rewrite the entry. Leave non-default paths alone (the
    // user explicitly customised them).
    auto migrateUserKey = [&settings, &resDir](const QString &key, const QString &basename)
    {
        QString stored = settings.value(key).toString();
        if (stored.isEmpty()) return;
        QString oldDefault = resDir + "/" + basename;
        QString newDefault = resDir + "/user/" + basename;
        if ((stored == oldDefault) && !QFile::exists(stored) && QFile::exists(newDefault))
            settings.setValue(key, newDefault);
    };
    migrateUserKey("colorsFile", "colors.json");
    for (const QString &key : settings.allKeys())
    {
        if (key.startsWith("waveform-"))
            migrateUserKey(key, key + ".json");
    }

    // Load tips before netnames.js so that custom-net comments in netnames.js
    // can augment or override entries already present in tips.json
    m_tips.load(resDir + "/user/tips.json");

    // Pick the colors file: persisted choice from the previous session if it
    // still exists, otherwise the bundled default in the resource directory.
    QString colorsFile = settings.value("colorsFile").toString();
    if (colorsFile.isEmpty() || !QFile::exists(colorsFile))
        colorsFile = resDir + "/user/colors.json";

    // Initialize all global classes using the given path to resource
    if (!m_simz80.loadResources(resDir) || !m_colors.load(colorsFile) || !m_chip.loadChipResources(resDir) || !m_simz80.initChip())
    {
        qCritical() << "Unable to load chip resources from" << resDir;
        return false;
    }
#if USE_AVX2_SIM
    // Initialize the AVX2 optimized simulator
    qInfo() << "Initializing AVX2 optimized simulator...";
    if (!m_simz80avx2.loadResources(resDir) || !m_simz80avx2.initChip())
    {
        qCritical() << "Unable to initialize AVX2 optimized simulator from" << resDir;
        return false;
    }
    qInfo() << "AVX2 optimized simulator initialized successfully";
#endif

    m_watch.load(resDir + "/user/watchlist.json");
    connect(this, &ClassController::eventNetName, &m_watch, &ClassWatch::onNetName);

    m_annotate.load(resDir + "/user/annotations.json");

    // Build the save registry. Each item reports its target paths through a function rather than a
    // fixed string because the paths are not fixed: most classes cache the name of the file they
    // were last loaded from, which a file dropped onto the application changes.
    m_saveItems.append({ "annotations", "Annotations",
        [this]() { return QStringList { m_annotate.getFileName() }; },
        [this](bool, QString &) { return m_annotate.save(m_annotate.getFileName()) ? SaveWritten : SaveFailed; } });

    // Net names, the buses and the per-net comments are one thing to the user but two files on
    // disk: saveCustomNames() writes each comment into netnames.js as a trailing "// tip" and emits
    // the bus definitions, while tips.json is the standalone comment store. Saving one without the
    // other leaves the two disagreeing, so this item always writes both.
    m_saveItems.append({ "netnames", "Net names, buses & comments",
        [this]() { QSettings s; return QStringList { s.value("ResourceDir").toString() + "/chip/netnames.js", m_tips.getFileName() }; },
        [this](bool, QString &reason) -> SaveOutcome
        {
            bool names = m_simz80.saveCustomNames();
            bool tips = m_tips.save(m_tips.getFileName());
            if (names && tips)
                return SaveWritten;
            // Name the half that failed: one of these two files may well be writable when the other is not
            reason = QString("unable to write %1").arg(!names && !tips ? QString("netnames.js and tips.json")
                                                                      : (names ? QString("tips.json") : QString("netnames.js")));
            return SaveFailed;
        } });

    m_saveItems.append({ "colors", "Colors",
        [this]() { return QStringList { m_colors.getFileName() }; },
        [this](bool automatic, QString &reason) -> SaveOutcome
        {
            QSettings s;
            s.setValue("colorsFile", m_colors.getFileName());
            // The inhibit flag guards a merge the user has not committed against the unattended
            // save on the way out. An explicit request is the user committing it, so it writes,
            // and ClassColors::save() clears the flag.
            if (automatic && m_colors.inhibitAutoSave())
            {
                reason = "merged colors were not committed, so the file on disk is left as it was";
                return SaveSkipped;
            }
            return m_colors.save(m_colors.getFileName()) ? SaveWritten : SaveFailed;
        } });

    m_saveItems.append({ "watchlist", "Watchlist",
        [this]() { return QStringList { m_watch.getFileName() }; },
        [this](bool, QString &) { return m_watch.save(m_watch.getFileName()) ? SaveWritten : SaveFailed; } });

    // Waveform views are created on demand from the Window menu, up to four. Their rows exist from
    // the start so the dialog can show every slot, but stay unavailable until a dock attaches.
    for (uint i = 1; i <= 4; i++)
        m_saveItems.append({ QString("waveform-%1").arg(i), QString("Waveform %1").arg(i), {}, {} });

    // Initialize the schematic generation properties
    DialogEditSchematic::init();

    // Both background servers are runtime-configurable through Edit > Settings...
    // The defaults below come from AppTypes.h; QSettings overrides win once the user has made a choice.
    applyServerSettings();

    // Execute init.js initialization script
    QTimer::singleShot(1000, [=]() { m_script.exec(R"(load("scripts/init.js"))"); });

    return true;
}

/*
 * Shut down long-running background services while qApp is still alive.
 * Called explicitly from main() after a.exec() returns; safe to call multiple
 * times. ClassController is a global, so its QObject children would otherwise
 * be destroyed after QApplication is gone — tearing down QHttpServer /
 * QTcpServer at that point trips "Must construct a QApplication before a
 * QWidget" on exit.
 */
void ClassController::stopServers()
{
    if (m_mcpServer) m_mcpServer->stop();
    m_server.stopListening();
}

/*
 * Reads the four server-related QSettings keys and reconciles the running state of each server. Called
 * once from init() to bring the servers up at startup, and again from DialogSettings::accept() whenever
 * the user changes a value. Each server is stopped if it was running on the wrong port or is now
 * disabled, and (re)started on the configured port if currently enabled.
 */
void ClassController::applyServerSettings()
{
    QSettings settings;
    bool socketEnable = settings.value("socketServer", SOCKET_SERVER).toBool();
    quint16 socketPort = quint16(settings.value("socketPort", SOCKET_PORT).toInt());
    bool mcpEnable = settings.value("mcpServer", MCP_SERVER).toBool();
    quint16 mcpPort = quint16(settings.value("mcpPort", MCP_PORT).toInt());

    // Reconcile the command socket server
    bool socketRunning = m_server.isListening();
    quint16 socketCurPort = socketRunning ? m_server.serverPort() : 0;
    if (socketRunning && (!socketEnable || (socketCurPort != socketPort)))
    {
        qInfo() << "Stopping command server on port" << socketCurPort;
        m_server.stopListening();
        socketRunning = false;
    }
    if (socketEnable && !socketRunning)
        startSocketServer(socketPort);

    // Reconcile the MCP server. The ClassMcpServer object is allocated lazily on first enable and kept
    // around afterwards (Qt parents own it); subsequent enable/disable just toggles its listener.
    bool mcpRunning = m_mcpServer && m_mcpServer->isListening();
    quint16 mcpCurPort = (m_mcpServer && mcpRunning) ? m_mcpServer->port() : 0;
    if (mcpRunning && (!mcpEnable || (mcpCurPort != mcpPort)))
    {
        qInfo() << "Stopping MCP server on port" << mcpCurPort;
        m_mcpServer->stop();
        mcpRunning = false;
    }
    if (mcpEnable && !mcpRunning)
    {
        if (!m_mcpTools)
        {
            m_mcpTools = new ClassMcpTools(this);
            m_mcpTools->registerDefaults();
        }
        if (!m_mcpServer)
        {
            m_mcpServer = new ClassMcpServer(m_mcpTools, this);
            QObject::connect(qApp, &QCoreApplication::aboutToQuit, m_mcpServer, &ClassMcpServer::stop);
        }
        if (m_mcpServer->start(mcpPort))
            qInfo() << "MCP server ready with" << m_mcpTools->toolCount() << "tools on port" << mcpPort;
        else
            qCritical() << "MCP server failed to start on port" << mcpPort;
    }
}

/*
 * Starts the command socket server and wires its commandReceived handler. The handler is connected
 * once per server-start (it is owned by the QTcpServer's child socket lifecycle), so re-entering this
 * function on a port change cleanly re-creates the connection.
 */
void ClassController::startSocketServer(quint16 port)
{
    if (!m_server.startListening(port))
    {
        qCritical() << "Failed to start command server on port" << port;
        return;
    }
    qInfo() << "Command server is listening on port" << m_server.serverPort();
    qInfo() << "Send text commands (one per line) to this port.";

    connect(&m_server, &ClassServer::commandReceived, this, [this](const QString &command, QTcpSocket *sock)
    {
        qDebug() << "Processing command:" << command;
        // Capture any text the script emits via ClassScript::print during this command so we can
        // forward it back to the socket client. DockCommand and the log still receive the same signal
        // in parallel, so on-screen output is unaffected
        QStringList captured;
        auto conn = connect(&m_script, &ClassScript::print, this,
            [&captured](QString msg) { captured.append(msg); });
        m_script.exec(command, /*echo=*/false);
        disconnect(conn);
        QByteArray reply;
        for (const QString &m : captured)
        {
            reply.append(m.toUtf8());
            if (!m.endsWith('\n'))
                reply.append('\n');
        }
        reply.append("OK\n");
        sock->write(reply);
    });
}

/*
 * Runs the chip reset sequence, returns the number of clocks thet reset took
 */
uint ClassController::doReset()
{
    qDebug() << "Chip reset";
    m_watch.clear(); // Clear watch signal history
    m_trick.reset(); // Reset the control counters etc.
#if USE_AVX2_SIM
    uint hcycle = m_simz80avx2.doReset();
#else
    uint hcycle = m_simz80.doReset();
#endif
    emit onRunStopped(hcycle);
    return hcycle;
}

/*
 * Runs the simulation for the given number of clocks
 */
void ClassController::doRunsim(uint ticks)
{
#if USE_AVX2_SIM
    m_simz80avx2.doRunsim(ticks);
#else
    m_simz80.doRunsim(ticks);
#endif

    if (ticks == INT_MAX)
        qInfo() << "Starting simulation";
    else if (ticks)
        qInfo() << "Simulation run for" << ticks << "half-clocks";
    else
        qInfo() << "Simulation stopped";
}

/*
 * Returns a list of formats applicable to the signal name (a net or a bus)
 */
const QStringList ClassController::getFormats(QString name)
{
    static const QStringList formats[2] = {
        { "Logic", "Logic (fill 0)", "Logic (fill 1)", "Transition Up", "Transition Down", "Transition Any" },
        { "Hexadecimal", "Binary", "Octal", "Decimal", "ASCII", "Disasm", "Ones' Complement" }
    };
    // If the name represents a bus, get() will return 0 for the net number, selecting formats[!0]
    return formats[!getNetlist().get(name)];
}

/*
 * Returns the formatted string for a bus type value
 * If decorated is true (default), the numbers will have Verilog-style prefix specifying the given width
 */
const QString ClassController::formatBus(uint fmt, uint value, uint width, bool decorated)
{
    if (Q_UNLIKELY(value == UINT_MAX)) return "hi-Z";
    if (fmt == FormatBus::OnesComplement)
        value = (~value) & ((1 << width) - 1);
    QString s = (decorated ? (QString::number(width) % "'h") : QString()) % QString::number(value, 16).toUpper(); // Print hex by default
    // Handle a special case where asked to print ASCII, but a value is not a prinable character: return its hex value
    if (Q_UNLIKELY((fmt == FormatBus::Ascii) && !QChar::isPrint(value)))
        return s;
    // Handle a special case where asked to print Disasm, but the width is not exactly 8 bits: return its hex value
    if (Q_UNLIKELY((fmt == FormatBus::Disasm) && (width != 8)))
        return s;
    static bool wasED = false;
    static bool wasCB = false;
    switch (fmt)
    {
        case FormatBus::Bin: s = (decorated ? (QString::number(width) % "'b") : QString()) % QString::number(value, 2);  break;
        case FormatBus::Oct: s = (decorated ? (QString::number(width) % "'o") : QString()) % QString::number(value, 8);  break;
        case FormatBus::Dec: s = (decorated ? (QString::number(width) % "'d") : QString()) % QString::number(value, 10); break;
        case FormatBus::Ascii: s = "'" % QChar(value) % "'"; break;
        case FormatBus::Disasm:
            // HACK: To get better opcode decode we keep the last ED/CB assuming we are called in-order
            s = z80state::disasm(value, !wasED, !wasCB);
            if (value != 0x00) // NOP keeps the flags as-is since there is a one-cycle NOP after ED/CB
                wasED = (value == 0xED), wasCB = (value == 0xCB);
            break;
    }
    if (fmt == FormatBus::OnesComplement)
        return "~" % s; // 1s-complement format shows tilde to accentuate it
    return s;
}

/*
 * Sets the name (alias) for a net.
 * In AVX2 mode the sim keeps its own name hash, so we update both.
 */
void ClassController::setNetName(const QString name, const net_t net)
{
    m_simz80.eventNetName(Netop::SetName, name, net);
#if USE_AVX2_SIM
    m_simz80avx2.eventNetName(Netop::SetName, name, net);
#endif
    emit eventNetName(Netop::SetName, name, net);
    emit eventNetName(Netop::Changed, QString(), net);
}

/*
 * Renames a net using the new name
 * In AVX2 mode the sim keeps its own name hash, so we update both.
 */
void ClassController::renameNet(const QString name, const net_t net)
{
    m_simz80.eventNetName(Netop::Rename, name, net);
#if USE_AVX2_SIM
    m_simz80avx2.eventNetName(Netop::Rename, name, net);
#endif
    emit eventNetName(Netop::Rename, name, net);
    emit eventNetName(Netop::Changed, QString(), net);
}

/*
 * Deletes the current name of a specified net
 * In AVX2 mode the sim keeps its own name hash, so we update both.
 */
void ClassController::deleteNetName(const net_t net)
{
    m_simz80.eventNetName(Netop::DeleteName, QString(), net);
#if USE_AVX2_SIM
    m_simz80avx2.eventNetName(Netop::DeleteName, QString(), net);
#endif
    emit eventNetName(Netop::DeleteName, QString(), net);
    emit eventNetName(Netop::Changed, QString(), net);
}

/*
 * Saves the named registry items, or every currently available one when the list is empty. Returns
 * one result per item asked for, so a caller can report exactly which files were written and which
 * were not. Safe to call while the simulation is running: the sim thread writes only into the
 * per-net sample buffers, never into the containers or the name and bus tables serialized here.
 */
QVector<ClassController::SaveResult> ClassController::save(const QStringList &ids, bool automatic)
{
    QStringList wanted = ids;
    if (wanted.isEmpty())
    {
        for (const SaveItem &item : std::as_const(m_saveItems))
        {
            if (item.save)
                wanted.append(item.id);
        }
    }

    QVector<SaveResult> results;
    for (const QString &id : std::as_const(wanted))
    {
        SaveResult r;
        r.id = id;
        bool known = false;
        std::function<QStringList()> files;      // Taken by value: the registry must not be read
        std::function<SaveOutcome(bool, QString &)> write; // across the call that writes the files
        for (const SaveItem &item : std::as_const(m_saveItems))
        {
            if (item.id != id)
                continue;
            known = true;
            files = item.files;
            write = item.save;
            break;
        }
        if (!known)
            r.reason = QString("'%1' is not a known save item").arg(id);
        else if (!write)
            r.reason = QString("'%1' has nothing to save yet").arg(id);
        else
        {
            const QStringList targets = files ? files() : QStringList();
            r.outcome = write(automatic, r.reason);
            // Only a write reports files, so nothing downstream can name a path it did not touch
            if (r.outcome == SaveWritten)
                r.files = targets;
            else if (r.reason.isEmpty())
                r.reason = QString("unable to write %1").arg(targets.join(", "));
        }
        results.append(r);
    }
    return results;
}

/*
 * Backs a registry item that was pre-registered without one, making it available to save. Used by
 * the waveform docks, which come into existence only when the user opens them.
 */
void ClassController::attachSaveItem(const QString &id, std::function<QStringList()> files,
                                     std::function<SaveOutcome(bool automatic, QString &reason)> save)
{
    for (SaveItem &item : m_saveItems)
    {
        if (item.id != id)
            continue;
        item.files = files;
        item.save = save;
        return;
    }
    qWarning() << "attachSaveItem: no save item with id" << id;
}

void ClassController::detachSaveItem(const QString &id)
{
    for (SaveItem &item : m_saveItems)
    {
        if (item.id != id)
            continue;
        item.files = {};
        item.save = {};
        return;
    }
    // A miss leaves callbacks in the registry bound to an object that is going away, so it must be
    // loud rather than silent: the next save would call into freed memory
    qWarning() << "detachSaveItem: no save item with id" << id;
}

/*
 * Compresses a file, writing the result into another file
 */
bool ClassController::compressFile(const QString &inFileName, const QString &outFileName)
{
    QFile file(inFileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Could not open file for reading.";
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QByteArray compressedData = qCompress(fileData);

    QFile compressedFile(outFileName);
    if (!compressedFile.open(QIODevice::WriteOnly))
    {
        qWarning() << "Could not open file for writing.";
        return false;
    }

    QDataStream out(&compressedFile);
    out << compressedData;
    compressedFile.close();

    return true;
}

/*
 * Uncompresses a file, writing the result into another file
 */
bool ClassController::uncompressFile(const QString &inFileName, const QString &outFileName)
{
    QFile compressedFile(inFileName);
    if (!compressedFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Could not open file for reading.";
        return false;
    }

    QByteArray compressedData;
    QDataStream in(&compressedFile);
    in >> compressedData;
    compressedFile.close();

    QByteArray uncompressedData = qUncompress(compressedData);

    QFile uncompressedFile(outFileName);
    if (!uncompressedFile.open(QIODevice::WriteOnly))
    {
        qWarning() << "Could not open file for writing.";
        return false;
    }

    uncompressedFile.write(uncompressedData);
    uncompressedFile.close();

    return true;
}
