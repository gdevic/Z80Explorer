#include "ClassScript.h"
#include <ClassController.h>
#include <algorithm>
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
    QScriptValue funDriving = m_engine->newFunction(&ClassScript::onDriving);
    m_engine->globalObject().setProperty("driving", funDriving);
    QScriptValue funDriven = m_engine->newFunction(&ClassScript::onDriven);
    m_engine->globalObject().setProperty("driven", funDriven);
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
    text << "driving(net)  - Shows a list of nets that the given net is driving\n";
    text << "driven(net)   - Shows a list of nets that drive the given net\n";
    text << "ex(n)         - Runs experimental function 'n'\n";
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

QScriptValue ClassScript::onDriving(QScriptContext *ctx, QScriptEngine *)
{
    net_t net = ctx->argument(0).toNumber();
    QVector<net_t> nets = ::controller.getNetlist().netsDriving(net);
    std::sort(nets.begin(), nets.end());
    QString s;
    for (auto n : nets)
        s += QString::number(n) + " ";
    emit ::controller.getScript().response(s);
    return "OK";
}

QScriptValue ClassScript::onDriven(QScriptContext *ctx, QScriptEngine *)
{
    net_t net = ctx->argument(0).toNumber();
    QVector<net_t> nets = ::controller.getNetlist().netsDriven(net);
    std::sort(nets.begin(), nets.end());
    QString s;
    for (auto n : nets)
        s += QString::number(n) + " ";
    emit ::controller.getScript().response(s);
    return "OK";
}

QScriptValue ClassScript::onExperimental(QScriptContext *ctx, QScriptEngine *)
{
    uint n = ctx->argument(0).toNumber();
    qDebug() << n;
    emit ::controller.getChip().experimental(n);
    return "OK";
}
