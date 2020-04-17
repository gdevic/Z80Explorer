#include "ClassScript.h"
#include <ClassController.h>
#include <QDebug>

ClassScript::ClassScript(QObject *parent) : QObject(parent)
{
}

void ClassScript::init(QScriptEngine *sc)
{
    m_engine = sc;

    QScriptValue funHelp = m_engine->newFunction(&ClassScript::onHelp);
    m_engine->globalObject().setProperty("help", funHelp);
    QScriptValue funRun = m_engine->newFunction(&ClassScript::onRun);
    m_engine->globalObject().setProperty("run", funRun);
    QScriptValue funStop = m_engine->newFunction(&ClassScript::onStop);
    m_engine->globalObject().setProperty("stop", funStop);
    QScriptValue funReset = m_engine->newFunction(&ClassScript::onReset);
    m_engine->globalObject().setProperty("reset", funReset);
    QScriptValue funExperimental = m_engine->newFunction(&ClassScript::onExperimental);
    m_engine->globalObject().setProperty("ex", funExperimental);
}

void ClassScript::run(QString cmd)
{
    emit response(m_engine->evaluate(cmd).toString());
}

QScriptValue ClassScript::onHelp(QScriptContext *, QScriptEngine *)
{
    QString s;
    QTextStream text(&s);
    text << "run(cycles)   - Runs the simulation for the given number of clocks\n";
    text << "stop()        - Stops the running simulation\n";
    text << "reset()       - Resets the simulation state\n";
    emit ::controller.getScript().response(s);
    return "OK";
}

QScriptValue ClassScript::onRun(QScriptContext *ctx, QScriptEngine *)
{
    uint cycles = ctx->argument(0).toNumber();
    emit ::controller.doRunsim(cycles);
    return "OK";
}

QScriptValue ClassScript::onStop(QScriptContext *, QScriptEngine *)
{
    emit ::controller.doRunsim(0);
    return "OK";
}

QScriptValue ClassScript::onReset(QScriptContext *, QScriptEngine *)
{
    emit ::controller.doReset();
    return "OK";
}

QScriptValue ClassScript::onExperimental(QScriptContext *ctx, QScriptEngine *)
{
    uint n = ctx->argument(0).toNumber();
    qDebug() << n;
    if (n == 1) emit ::controller.getChip().drawExperimental();
    return "OK";
}
