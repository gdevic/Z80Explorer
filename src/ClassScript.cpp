#include "ClassScript.h"
#include <ClassController.h>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QProcess>

ClassScript::ClassScript(QObject *parent) : QObject(parent)
{}

void ClassScript::init(QJSEngine *sc)
{
    m_engine = sc;

    QJSValue ext = m_engine->globalObject().property("script"); // "this"

    // Export these functions to the root of the global JS namespace
    m_engine->globalObject().setProperty("load", ext.property("load"));
    m_engine->globalObject().setProperty("run", ext.property("run"));
    m_engine->globalObject().setProperty("stop", ext.property("stop"));
    m_engine->globalObject().setProperty("reset", ext.property("reset"));
    m_engine->globalObject().setProperty("t", ext.property("t"));
    m_engine->globalObject().setProperty("n", ext.property("n"));
    m_engine->globalObject().setProperty("eq", ext.property("eq"));
    m_engine->globalObject().setProperty("print", ext.property("print"));
    m_engine->globalObject().setProperty("relatch", ext.property("relatch"));
    m_engine->globalObject().setProperty("save", ext.property("save"));
    m_engine->globalObject().setProperty("ex", ext.property("ex"));
    m_engine->globalObject().setProperty("execApp", ext.property("execApp"));
    m_engine->globalObject().setProperty("readBit", ext.property("readBit"));
    m_engine->globalObject().setProperty("readByte", ext.property("readByte"));
    m_engine->globalObject().setProperty("readBits", ext.property("readBits"));
    m_engine->globalObject().setProperty("getMTState", ext.property("getMTState"));
    m_engine->globalObject().setProperty("saveText", ext.property("saveText"));
    m_engine->globalObject().setProperty("setNetName", ext.property("setNetName"));
    m_engine->globalObject().setProperty("saveNetnames", ext.property("saveNetnames"));
}

/*
 * Loads (imports) a script
 * If no script name was given, load a default "script.js"
 */
void ClassScript::load(QString fileName)
{
    fileName = fileName.trimmed();
    if (fileName.isEmpty())
        fileName = "script.js";
    qInfo() << "Loading script" << fileName;

    QFile scriptFile(fileName);
    if (!scriptFile.open(QIODevice::ReadOnly))
        m_engine->throwError(QString("%1: %2").arg(fileName, scriptFile.errorString()));
    else
    {
        QTextStream stream(&scriptFile);
        QString program = stream.readAll();
        scriptFile.close();

        exec(program, false);
    }
}

/*
 * Command line execution of built-in scripting
 */
void ClassScript::exec(QString cmd, bool echo)
{
    if (echo)
        emit ::controller.getScript().print(cmd);

    // TODO: Make JS evaluate in a different thread (QtConcurrent::run())
    // If you need to execute potentially long-running JavaScript, you'll need to do it from a separate thread with QJS
    QJSValue result = m_engine->evaluate(cmd);
    if (result.isError())
        emit ::controller.getScript().print(QString("Exception at line %1 : %2").arg(result.property("lineNumber").toInt()).arg(result.toString()));
    else
    {
        if (!result.isUndefined())
            emit ::controller.getScript().print(result.toString());
    }
}

void ClassScript::run(uint hcycles)
{
    ::controller.doRunsim(hcycles ? hcycles : INT_MAX);
}

void ClassScript::stop()
{
    ::controller.doRunsim(0);
}

void ClassScript::reset()
{
    ::controller.doReset();
}

void ClassScript::t(uint n)
{
    QString s = ::controller.getNetlist().transInfo(n);
    emit ::controller.getScript().print(s);
}

void ClassScript::n(QVariant n)
{
    bool ok = false;

    net_t net = n.toUInt(&ok);
    if (!ok)
    {
        QString name = n.toString();
        net = ::controller.getNetlist().get(name);
    }
    QString s = ::controller.getNetlist().netInfo(net);
    emit ::controller.getScript().print(s);
}

void ClassScript::eq(QVariant n)
{
    bool ok = false;

    net_t net = n.toUInt(&ok);
    if (!ok)
    {
        QString name = n.toString();
        net = ::controller.getNetlist().get(name);
    }
    QString s = ::controller.getNetlist().equation(net);
    emit ::controller.getScript().print(s);
}

/*
 * Rebuilds latches; reloads custom latches
 */
void ClassScript::relatch()
{
    ::controller.getChip().detectLatches();
}

/*
 * Experimental functions
 */
void ClassScript::ex(uint n)
{
    qDebug() << n;
    ::controller.getChip().experimental(n);
}

/*
 * Run external application
 * Returns a QVariantMap with success status, output, error, and exit code
 */
QJSValue ClassScript::execApp(const QString &path, const QStringList &args, bool synchronous)
{
    QCoreApplication::processEvents(); // Refresh the GUI to show preceding output from init.js:exec

    QJSValue result = m_engine->newObject();
    result.setProperty("success", false);
    result.setProperty("stdout", "");
    result.setProperty("stderr", "");
    result.setProperty("errorString", "");
    result.setProperty("exitCode", -1); // Default error exit code

    // Using a new QProcess for each call is generally safer and simpler for this use case
    QProcess process(this);

    qDebug() << "Attempting to run:" << path << "with arguments:" << args;
    qDebug() << "Synchronous:" << synchronous;

    if (synchronous)
    {
        // To get stdout/stderr reliably in synchronous mode, use start() and waitForFinished()
        process.setProgram(path);
        process.setArguments(args);

        process.start();

        if (!process.waitForStarted())
        {
            qWarning() << "Failed to start process:" << process.errorString();
            result.setProperty("errorString", process.errorString());
            result.setProperty("success", false);
            return result;
        }

        if (process.waitForFinished(-1)) // -1 waits indefinitely
        {
            result.setProperty("stdout", QString::fromUtf8(process.readAllStandardOutput()));
            result.setProperty("stderr", QString::fromUtf8(process.readAllStandardError()));
            result.setProperty("exitCode", process.exitCode());
            result.setProperty("success", (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0));
            qDebug() << "Synchronous execution finished. Exit code:" << process.exitCode();
            qDebug() << "Stdout:" << result.property("stdout").toString();
            qDebug() << "Stderr:" << result.property("stderr").toString();
        }
        else
        {
            qWarning() << "Synchronous execution failed or timed out:" << process.errorString();
            result.setProperty("errorString", process.errorString());
            result.setProperty("stderr", QString::fromUtf8(process.readAllStandardError())); // Capture any error output
        }
    }
    else // Asynchronous
    {
        // For asynchronous, we start and don't wait.
        // The script won't get immediate feedback other than if 'start' itself failed.
        // This function, as designed for a direct JS call, can only indicate if 'start' was successful.
        process.setProgram(path);
        process.setArguments(args);

        bool started = process.startDetached(); // Simpler for fire-and-forget async

        if (started)
        {
            qDebug() << "Asynchronous process started successfully (detached).";
            result.setProperty("success", true);
            result.setProperty("message", "Process started asynchronously (detached).");
            // Note: For detached processes, getting exit code or output directly back in this call is not possible.
        }
        else
        {
            qWarning() << "Failed to start asynchronous process:" << process.errorString();
            result.setProperty("errorString", process.errorString());
            result.setProperty("success", false);
        }
    }
    return result;
}

// Read net values from the live simulator (AVX2 build uses a separate netlist
// instance from ClassNetlist, so we must go through getSimZ80()).
#if USE_AVX2_SIM
#  define LIVE_SIM (::controller.getSimZ80())
#else
#  define LIVE_SIM (::controller.getNetlist())
#endif

/*
 * Returns the current logic value of a single net: 0, 1, or 2 (hi-Z).
 *
 * Accepts either a net NAME (looked up via the live netlist hash) or a stringified net
 * NUMBER (e.g. "1239") which is read directly. Numeric lookup is the escape hatch used by
 * the socket probe client to read any net by number without touching the name hash, which
 * matters in AVX2 mode where the AVX2 sim maintains its own name hash that setNetName doesn't update.
 *
 * Returns -1 if the net name is unknown or the number is out of range.
 */
int ClassScript::readBit(const QString &name)
{
    auto &nl = LIVE_SIM;
    // Numeric-string fast path: interpret "1239" as net number 1239.
    bool isNum = false;
    uint num = name.toUInt(&isNum);
    if (isNum)
    {
        if (num >= MAX_NETS)
            return -1;
        return static_cast<int>(nl.readBit(static_cast<net_t>(num)));
    }
    net_t n = nl.get(name);
    if ((n == 0) && (name != "vss") && (name != "gnd"))
        return -1;
    return static_cast<int>(nl.readBit(n));
}

/*
 * Reads an 8-bit value from a bus whose bits are named <base>0..<base>7.
 */
int ClassScript::readByte(const QString &base)
{
    return static_cast<int>(LIVE_SIM.readByte(base));
}

/*
 * Batched net read. For each name, emits its current bit value (0/1/2) or '-' if the net is unknown.
 * Numeric strings (e.g. "1239") are treated as net numbers and read directly without a name lookup.
 * The result is a single CSV string so that large per-half-cycle captures avoid QJSValue overhead.
 */
QString ClassScript::readBits(const QStringList &names)
{
    auto &nl = LIVE_SIM;
    QString out;
    out.reserve(names.size() * 2);
    for (int i = 0; i < names.size(); ++i)
    {
        if (i > 0)
            out.append(QLatin1Char(','));
        const QString &name = names.at(i);
        bool isNum = false;
        uint num = name.toUInt(&isNum);
        if (isNum)
        {
            if (num >= MAX_NETS)
                out.append(QLatin1Char('-'));
            else
                out.append(QString::number(static_cast<int>(nl.readBit(static_cast<net_t>(num)))));
            continue;
        }
        net_t n = nl.get(name);
        if ((n == 0) && (name != "vss") && (name != "gnd"))
            out.append(QLatin1Char('-'));
        else
            out.append(QString::number(static_cast<int>(nl.readBit(n))));
    }
    return out;
}

/*
 * Returns the current machine-cycle / T-state as a short string, e.g. "M1T2".
 * If no latch in a row is set, its digit is '?'. Convenience wrapper so capture
 * scripts do not repeat the latch scan in JS.
 */
QString ClassScript::getMTState()
{
    auto &nl = LIVE_SIM;
    QChar m = QLatin1Char('?');
    for (int i = 1; i <= 6; ++i)
    {
        net_t n = nl.get(QString("m%1").arg(i));
        if (n && nl.readBit(n) == 1)
        {
            m = QLatin1Char('0' + i);
            break;
        }
    }
    QChar t = QLatin1Char('?');
    for (int i = 1; i <= 6; ++i)
    {
        net_t n = nl.get(QString("t%1").arg(i));
        if (n && nl.readBit(n) == 1)
        {
            t = QLatin1Char('0' + i);
            break;
        }
    }
    return QString("M%1T%2").arg(m).arg(t);
}

/*
 * Writes 'content' to a text file at 'path' (UTF-8). Creates the file, overwriting if it exists.
 * Returns true on full successful write.
 */
bool ClassScript::saveText(const QString &path, const QString &content)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "saveText: cannot open" << path << ":" << f.errorString();
        return false;
    }
    const QByteArray bytes = content.toUtf8();
    const qint64 written = f.write(bytes);
    f.close();
    return written == bytes.size();
}

/*
 * Assigns 'name' to the given net number. Forwards to ClassController::setNetName
 * which dispatches the name-change event through the normal eventNetName pipeline,
 * so the name is picked up by ClassNetlist and persists on the next save().
 */
void ClassScript::setNetName(const QString &name, uint net)
{
    ::controller.setNetName(name, static_cast<net_t>(net));
}

/*
 * Checkpoint netnames.js from a running probe script, so a long-running
 * discovery run can persist each find without having to quit the app.
 */
bool ClassScript::saveNetnames()
{
    return ::controller.getNetlist().saveCustomNames();
}
