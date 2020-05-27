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
    QScriptValue funDriving = m_engine->newFunction(&ClassScript::onDriving);
    m_engine->globalObject().setProperty("driving", funDriving);
    QScriptValue funDriven = m_engine->newFunction(&ClassScript::onDriven);
    m_engine->globalObject().setProperty("driven", funDriven);
    QScriptValue funNet = m_engine->newFunction(&ClassScript::onNet);
    m_engine->globalObject().setProperty("n", funNet);
    QScriptValue funTrans = m_engine->newFunction(&ClassScript::onTrans);
    m_engine->globalObject().setProperty("t", funTrans);
    QScriptValue funExperimental = m_engine->newFunction(&ClassScript::onExperimental);
    m_engine->globalObject().setProperty("ex", funExperimental);
    QScriptValue funLoad = m_engine->newFunction(&ClassScript::onLoad);
    m_engine->globalObject().setProperty("load", funLoad);
}

/*
 * Command line execution of built-in scripting
 */
void ClassScript::run(QString cmd)
{
    m_code += cmd + QLatin1Char('\n');
    if (!m_engine->canEvaluate(m_code))
        emit response("...");
    else
    {
        // XXX I am not quite sure exactly how this should flow
        QScriptValue result = m_engine->evaluate(cmd, cmd);
        m_code.clear();
        if (!result.isUndefined())
            emit response(result.toString());
        if (result.isError())
            emit response( QString::fromLatin1("%0:%1: %2").arg(cmd).arg(result.property("lineNumber").toInt32()).arg(result.toString()));

    }
}

QScriptValue ClassScript::onHelp(QScriptContext *, QScriptEngine *)
{
    QString s;
    QTextStream text(&s);
    text << "run(cycles)   - Runs the simulation for the given number of clocks\n";
    text << "stop()        - Stops the running simulation\n";
    text << "reset()       - Resets the simulation state\n";
    text << "t(t)          - Shows a transistor state\n";
    text << "n(net)        - Shows a net state by net number or \"name\"\n";
    text << "driving(net)  - Shows a list of nets that the given net is driving\n";
    text << "driven(net)   - Shows a list of nets that drive the given net\n";
    text << "ex(n)         - Runs experimental function 'n'\n";
    text << "load(file)    - Executes a script file ('script.js' by default)\n";
    text << "In addition, objects 'control', 'sim', 'monitor' and 'script' provide methods described in the documentation.";
    emit ::controller.getScript().response(s);
    return "OK";
}

QScriptValue ClassScript::onRun(QScriptContext *ctx, QScriptEngine *)
{
    uint cycles = ctx->argument(0).toNumber();
    ::controller.doRunsim(cycles);
    return "OK";
}

QScriptValue ClassScript::onStop(QScriptContext *, QScriptEngine *)
{
    ::controller.doRunsim(0);
    return "OK";
}

QScriptValue ClassScript::onReset(QScriptContext *, QScriptEngine *)
{
    ::controller.doReset();
    return "OK";
}

QScriptValue ClassScript::onDriving(QScriptContext *ctx, QScriptEngine *)
{
    net_t net = ctx->argument(0).toNumber();
    QString name = ctx->argument(0).toString();
    if (!net)
        net = ::controller.getNetlist().get(name);
    QVector<net_t> nets = ::controller.getNetlist().netsDriving(net);
    QStringList list = ::controller.getNetlist().get(nets);
    emit ::controller.getScript().response(list.join(" "));
    return "OK";
}

QScriptValue ClassScript::onDriven(QScriptContext *ctx, QScriptEngine *)
{
    net_t net = ctx->argument(0).toNumber();
    QString name = ctx->argument(0).toString();
    if (!net)
        net = ::controller.getNetlist().get(name);
    QVector<net_t> nets = ::controller.getNetlist().netsDriven(net);
    QStringList list = ::controller.getNetlist().get(nets);
    emit ::controller.getScript().response(list.join(" "));
    return "OK";
}

QScriptValue ClassScript::onNet(QScriptContext *ctx, QScriptEngine *)
{
    net_t net = ctx->argument(0).toNumber();
    QString name = ctx->argument(0).toString();
    if (!net)
        net = ::controller.getNetlist().get(name);
    QString s = ::controller.getNetlist().netInfo(net);
    emit ::controller.getScript().response(s);
    return "OK";
}

QScriptValue ClassScript::onTrans(QScriptContext *ctx, QScriptEngine *)
{
    uint trans = ctx->argument(0).toNumber();
    QString s = ::controller.getNetlist().transInfo(trans);
    emit ::controller.getScript().response(s);
    return "OK";
}

QScriptValue ClassScript::onExperimental(QScriptContext *ctx, QScriptEngine *)
{
    uint n = ctx->argument(0).toNumber();
    qDebug() << n;
    ::controller.getChip().experimental(n);
    return "OK";
}

/*
 * Loads (imports) a script
 * If no script name was given, load a default "script.js"
 */
QScriptValue ClassScript::onLoad(QScriptContext *ctx, QScriptEngine *engine)
{
    QString fileName = ctx->argument(0).toString();
    if (fileName == "undefined")
        fileName = "script.js";
    qDebug() << "Running script" << fileName;

    QString error("OK");
    QFile scriptFile(fileName);
    if (scriptFile.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&scriptFile);
        QString contents = stream.readAll();
        scriptFile.close();

        QScriptContext *pc = ctx->parentContext();
        ctx->setActivationObject(pc->activationObject());
        ctx->setThisObject(pc->thisObject());

        QScriptValue result = engine->evaluate(contents, fileName);
        if (engine->hasUncaughtException())
            return result;

        // XXX I am not quite sure exactly how this should flow
        if (result.isError())
            error = QString::fromLatin1("%0:%1: %2").arg(fileName).arg(result.property("lineNumber").toInt32()).arg(result.toString());
    }
    else
        return ctx->throwError(QString("Could not open %0 for reading").arg(fileName));

    if (error != "OK")
    {
        qWarning() << error;
        return -1;
    }
    return "OK";
}
