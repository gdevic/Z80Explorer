#include "ClassScript.h"
#include <QtScript/QScriptEngine>
#include <QDebug>

ClassScript::ClassScript(QObject *parent) : QObject(parent)
{
}

/*
 * Evaluates and runs command
 */
void ClassScript::run(QString cmd)
{
    QScriptEngine engine;
    emit response(engine.evaluate(cmd).toString());
}
