#ifndef CLASSSCRIPT_H
#define CLASSSCRIPT_H

#include <QtScript>

/*
 * This class provides scripting functionality to the app
 */
class ClassScript : public QObject
{
    Q_OBJECT
public:
    explicit ClassScript(QObject *parent = nullptr);
    void init(QScriptEngine *sc);

signals:
    void response(QString);     // Write a response string to the command list

public slots:
    void run(QString cmd);      // Evaluates and runs command

private:
    static QScriptValue onHelp(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onRun(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onStop(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onReset(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onExperimental(QScriptContext *ctx, QScriptEngine *eng);

private:
    QScriptEngine *m_engine;
};

#endif // CLASSSCRIPT_H
