#include "ClassScript.h"
#include <ClassController.h>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QMetaEnum>
#include <QProcess>
#include <QtConcurrentRun>
#include <type_traits>

ClassScript::ClassScript(QObject *parent) : QObject(parent)
{
    Q_ASSERT(this->thread() == ::controller.thread());
}

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
 * Stops any running script evaluation (kills long-running scripts)
 */
void ClassScript::stopx()
{
    if (m_exec.isRunning())
    {
        bool wait = true;
        QEventLoop e; // don't block the GUI when waiting for the script to stop running
        qInfo() << "Stopping script evaluation...";
        m_exec.then([&e, &wait](QFuture<QJSValue>) {
            if (!e.thread()->isCurrentThread())
            {
                while (!e.isRunning())
                    QThread::yieldCurrentThread();
                e.exit();
            }
            else
                // we're executing on the calling thread, and there's no need to wait
                // once then() returns, there's nothing left running
                wait = false;
        });
        m_engine->setInterrupted(true);
        if (wait)
            e.exec();
        qInfo() << "Script evaluation has been stopped.";
    }
}

/*
 * Command line execution of built-in scripting
 */
void ClassScript::exec(QString cmd, bool echo)
{
    Q_ASSERT(thread()->isCurrentThread());

    if (echo)
        emit ::controller.getScript().print(cmd);

    if (m_exec.isRunning())
    {
        emit ::controller.getScript().print(QString("Error: a script or command is already executing."));
        return;
    }

    m_engine->setInterrupted(false);
    m_exec = QtConcurrent::run([this, cmd] { return execImpl(cmd); });
}

/************************************************************************************
 *                                                                                  *
 * All methods below are called from the worker thread in which the JS engine runs. *
 *                                                                                  *
 ************************************************************************************/

/*
 * Loads (imports) a script
 * If no script name was given, load a default "script.js"
 */
void ClassScript::load(QString fileName)
{
    Q_ASSERT(m_exec.isRunning());

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

        execImpl(program);
    }
}

/*
 * Command line execution of built-in scripting
 */
QJSValue ClassScript::execImpl(QString cmd)
{
#if 0
    if (echo)
        emit ::controller.getScript().print(cmd);
#endif

    Q_ASSERT(m_exec.isRunning());
    QJSValue result = m_engine->evaluate(cmd);
    if (result.isError())
        emit ::controller.getScript().print(
            QString("Exception at line %1 : %2").arg(result.property("lineNumber").toInt()).arg(result.toString()));
    else if (!result.isUndefined())
        emit ::controller.getScript().print(result.toString());
    return result;
}

void ClassScript::run(uint hcycles)
{
    QMetaObject::invokeMethod(&::controller, &ClassController::doRunsim, hcycles ? hcycles : INT_MAX);
}

void ClassScript::stop()
{
    QMetaObject::invokeMethod(&::controller, &ClassController::doRunsim, 0);
}

void ClassScript::reset()
{
    QMetaObject::invokeMethod(&::controller, &ClassController::doReset);
}

void ClassScript::t(uint n)
{
    // invoke ourselves from the main thread if we're not on the main thread
    if (!thread()->isCurrentThread())
        return (void) QMetaObject::invokeMethod(this, &ClassScript::t, n);

    QString s = ::controller.getNetlist().transInfo(n);
    emit ::controller.getScript().print(s);
}

void ClassScript::n(QVariant n)
{
    QMetaObject::invokeMethod(
        &::controller,
        [this](QVariant n) {
            bool ok = false;

            net_t net = n.toUInt(&ok);
            if (!ok)
            {
                QString name = n.toString();
                net = ::controller.getNetlist().get(name);
            }
            QString s = ::controller.getNetlist().netInfo(net);
            emit ::controller.getScript().print(s);
        },
        n);
}

void ClassScript::eq(QVariant n)
{
    QMetaObject::invokeMethod(
        this,
        [this](QVariant n) {
            bool ok = false;

            net_t net = n.toUInt(&ok);
            if (!ok)
            {
                QString name = n.toString();
                net = ::controller.getNetlist().get(name);
            }
            QString s = ::controller.getNetlist().equation(net);
            emit ::controller.getScript().print(s);
        },
        n);
}

/*
 * Rebuilds latches; reloads custom latches
 */
void ClassScript::relatch()
{
    QMetaObject::invokeMethod(&::controller, [] { ::controller.getChip().detectLatches(); });
}

/*
 * Experimental functions
 */
void ClassScript::ex(uint n)
{
    QMetaObject::invokeMethod(
        &::controller,
        [](uint n) {
            qDebug() << n;
            ::controller.getChip().experimental(n);
        },
        n);
}

template<typename Enum>
static const char *enumToKey(Enum value)
{
    return QMetaEnum::fromType<std::decay_t<Enum>>().valueToKey(value);
}

static QJSValue resultFromProcess(QProcess *process, QJSEngine *engine)
{
    QJSValue result = engine->newObject();
    result.setProperty("stdout", QString::fromUtf8(process->readAllStandardOutput()));
    result.setProperty("stderr", QString::fromUtf8(process->readAllStandardError()));
    result.setProperty("exitCode", process->exitCode());
    result.setProperty("success", (process->exitStatus() == QProcess::NormalExit && process->exitCode() == 0));
    if (process->exitStatus() != QProcess::NormalExit)
        result.setProperty("errorString", enumToKey(process->exitStatus()));
    result.setProperty("state", enumToKey(process->state()));
    return result;
}

/*
 * Run external application
 * Returns an object with success status, output, error, and exit code
 */
QJSValue ClassScript::execApp(const QString &path, const QStringList &args, const QJSValue &synchronous)
{
    bool syncIsTrue = synchronous.isBool() && synchronous.toBool();
    bool syncIsFalse = synchronous.isBool() && !synchronous.toBool();
    bool syncIsCallable = synchronous.isCallable();

    QJSValue result = m_engine->newObject();
    result.setProperty("success", false);
    result.setProperty("stdout", "");
    result.setProperty("stderr", "");
    result.setProperty("errorString", "");
    result.setProperty("exitCode", -1); // Default error exit code

    qDebug() << "Attempting to run:" << path << "with arguments:" << args;
    qDebug() << "Synchronous:"
             << (syncIsCallable ? "continuation"
                 : syncIsTrue   ? "true"
                 : syncIsFalse  ? "false"
                                : "invalid value");

    if (syncIsTrue)
    {
        // To get stdout/stderr reliably in synchronous mode, use start() and waitForFinished()
        QProcess process;
        process.start(path, args);

        if (!process.waitForStarted())
        {
            qWarning() << "Failed to start process:" << process.errorString();
            result.setProperty("errorString", process.errorString());
            result.setProperty("success", false);
            return result;
        }

        if (process.waitForFinished(-1)) // -1 waits indefinitely
        {
            QJSValue result = resultFromProcess(&process, m_engine);
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
        return result;
    }
    else if (syncIsFalse)
    {
        // For asynchronous, we start and don't wait.
        // The script won't get immediate feedback other than if 'start' itself failed.
        // This function, as designed for a direct JS call, can only indicate if 'start' was successful.

        QProcess process;
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
        return result;
    }
    else if (syncIsCallable)
    {
        // For a continuation, we capture the process and call into the javascript engine when it's done
        // The process must be running in the main thread, since that thread has a permanent event
        // loop that QProcess needs to make progress asynchronously.

        QMetaObject::invokeMethod(this, [this, path, args, callback = synchronous, result] {
            QProcess *process = new QProcess(this);

            QObject::connect(process, &QProcess::started, this, [this, process, protoResult = result, callback] {
                submitCall(callback, {"STARTED"});
            });

            QObject::connect(process, &QProcess::errorOccurred, this, [this, process, protoResult = result, callback] {
                QJSValue result = protoResult;
                result.setProperty("errorString", process->errorString());
                submitCall(callback, {result});
                process->deleteLater();
            });

            QObject::connect(process, &QProcess::finished, this, [this, process, protoResult = result, callback] {
                QJSValue result = resultFromProcess(process, m_engine);
                qDebug() << "Execution finished, continuation is about to be invoked. Exit code:"
                         << process->exitCode();
                qDebug() << "Stdout:" << result.property("stdout").toString();
                qDebug() << "Stderr:" << result.property("stderr").toString();
                submitCall(callback, {result});
                process->deleteLater();
            });

            process->start(path, args);
            return QJSValue();
        });
    }
    return {};
    // exec("c:/windows/system32/cmd.exe", ["/c", "exit"], function (result) { print(result, "123") })
}

// This class must be called from the main thread
void ClassScript::submitCall(QJSValue continuation, QJSValueList arguments)
{
    Q_ASSERT(thread()->isCurrentThread());

    // the future must have ran or is running, since the continuation can only be
    // invoked if we have submitted some work to the JS engine
    Q_ASSERT(m_exec.isRunning() || m_exec.isFinished());

    // QFuture::then is invoked whether the future is running or has already finished
    m_exec.then([this, continuation, arguments](QJSValue) {
        if (thread()->isCurrentThread())
            // the future has finished previously, we've been invoked from the main thread
            QFuture<void> result = QtConcurrent::run([this, continuation, arguments] { continuation.call(arguments); });
        else
            // the future has just finished and we're invoked from the thread where the engine has just finished
            // evaluation
            continuation.call(arguments);
    });
}
