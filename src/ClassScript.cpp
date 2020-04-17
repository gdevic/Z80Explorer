#include "ClassScript.h"
#include <ClassController.h>
#include <QDebug>

ClassScript::ClassScript(QObject *parent) : QObject(parent)
{
}

void ClassScript::init(QScriptEngine *sc)
{
    m_engine = sc;

    QScriptValue funRun = m_engine->newFunction(&ClassScript::onRun);
    m_engine->globalObject().setProperty("run", funRun);
    QScriptValue funStop = m_engine->newFunction(&ClassScript::onStop);
    m_engine->globalObject().setProperty("stop", funStop);
    QScriptValue funReset = m_engine->newFunction(&ClassScript::onReset);
    m_engine->globalObject().setProperty("reset", funReset);
}

/*
 * Evaluates and runs command
 */
void ClassScript::run(QString cmd)
{
    emit response(m_engine->evaluate(cmd).toString());
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
