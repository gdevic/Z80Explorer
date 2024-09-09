#include "ClassScript.h"
#include <ClassController.h>
#include <QDebug>
#include <QFile>

ClassScript::ClassScript(QObject *parent) : QObject(parent)
{
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
        m_engine->throwError(scriptFile.errorString());
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
