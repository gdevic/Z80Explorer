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
    m_engine->globalObject().setProperty("help", ext.property("help"));
    m_engine->globalObject().setProperty("run", ext.property("run"));
    m_engine->globalObject().setProperty("stop", ext.property("stop"));
    m_engine->globalObject().setProperty("reset", ext.property("reset"));
    m_engine->globalObject().setProperty("n", ext.property("n"));
    m_engine->globalObject().setProperty("t", ext.property("t"));
    m_engine->globalObject().setProperty("ex", ext.property("ex"));
    m_engine->globalObject().setProperty("relatch", ext.property("relatch"));
}

/*
 * Loads (imports) a script
 * If no script name was given, load a default "script.js"
 */
QJSValue ClassScript::load(QString fileName)
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

        // TODO: Make JS evaluate in a different thread (QtConcurrent::run())
        QJSValue result = m_engine->evaluate(program, fileName);
        if (result.isError())
            qDebug() << "Exception at line" << result.property("lineNumber").toInt() << ":" << result.toString();
    }
    return QJSValue();
}

/*
 * Stops any running script evaluation (kills long-running scripts)
 */
void ClassScript::stopx()
{
    // TODO: Make JS evaluate in a different thread (QtConcurrent::run())
    m_engine->setInterrupted(true);
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
void ClassScript::exec(QString cmd)
{
    // TODO: Make JS evaluate in a different thread (QtConcurrent::run())
    QJSValue result = m_engine->evaluate(cmd);
    if (result.isError())
        qDebug() << "Exception at line" << result.property("lineNumber").toInt() << ":" << result.toString();
    m_engine->setInterrupted(false);
    emit response(result.toString());
}

QJSValue ClassScript::help()
{
    static const QString s {
R"(run(hcycles)  - Runs the simulation for the given number of half-clocks
stop()        - Stops the running simulation
reset()       - Resets the simulation state
t(trans)      - Shows a transistor state
n(net|"name") - Shows a net state by net number or net "name"
ex(n)         - Runs experimental function "n"
load("file")  - Loads and executes a script file ("script.js" by default)
relatch()     - Reloads custom latches from "latches.ini" file
In addition, objects "control", "sim", "monitor", "script" and "img" provide methods described in the documentation.)" };

    emit ::controller.getScript().response(s);
    return QJSValue();
}

QJSValue ClassScript::run(uint hcycles)
{
    ::controller.doRunsim(hcycles ? hcycles : INT_MAX);
    return QJSValue();
}

QJSValue ClassScript::stop()
{
    ::controller.doRunsim(0);
    return QJSValue();
}

QJSValue ClassScript::reset()
{
    ::controller.doReset();
    return QJSValue();
}

QJSValue ClassScript::n(QVariant n)
{
    bool ok = false;

    net_t net = n.toUInt(&ok);
    if (!ok)
    {
        QString name = n.toString();
        net = ::controller.getNetlist().get(name);
    }
    QString s = ::controller.getNetlist().netInfo(net);
    emit ::controller.getScript().response(s);
    return QJSValue();
}

QJSValue ClassScript::t(uint n)
{
    QString s = ::controller.getNetlist().transInfo(n);
    emit ::controller.getScript().response(s);
    return QJSValue();
}

/*
 * Experimental functions
 */
QJSValue ClassScript::ex(uint n)
{
    qDebug() << n;
    ::controller.getChip().experimental(n);
    return QJSValue();
}

/*
 * Rebuilds latches; reloads custom latches
 */
QJSValue ClassScript::relatch()
{
    ::controller.getChip().detectLatches();
    return QJSValue();
}
