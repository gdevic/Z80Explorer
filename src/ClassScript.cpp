#include "ClassScript.h"
#include <QDebug>

ClassScript::ClassScript(QObject *parent) : QObject(parent)
{
}

void ClassScript::init(QScriptEngine *sc)
{
    m_engine = sc;
}

/*
 * Evaluates and runs command
 */
void ClassScript::run(QString cmd)
{
    emit response(m_engine->evaluate(cmd).toString());
}
