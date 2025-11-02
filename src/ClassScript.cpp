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
 * Stops any running script evaluation (kills long-running scripts)
 */
void ClassScript::stopx()
{
    // TODO: Make JS evaluate in a different thread (QtConcurrent::run())
    // m_engine->setInterrupted(true);
#if 0
    if (m_engine->isEvaluating())
    {
        qInfo() << "Stopping script evaluation";
        m_engine->abortEvaluation();
    }
#endif
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
