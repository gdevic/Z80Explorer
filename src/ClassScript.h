#ifndef CLASSSCRIPT_H
#define CLASSSCRIPT_H

#include <QtScript>

/*
 * This class provides scripting functionality to the app
 */
class ClassScript : public QObject
{
    Q_OBJECT                        //* <- Methods of the scripting object "script" below
public:
    explicit ClassScript(QObject *parent = nullptr);
    void init(QScriptEngine *sc);

signals:
    void response(QString);         //* Write a response string to the command list

public slots:
    void stop();                    // Stops any running script evaluation
    void exec(QString cmd);         //* Evaluates and runs commands

private:
    static QScriptValue onPrint(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onHelp(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onRun(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onStop(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onReset(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onNet(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onTrans(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onExperimental(QScriptContext *ctx, QScriptEngine *eng);
    static QScriptValue onLoad(QScriptContext *ctx, QScriptEngine *engine);
    static QScriptValue onRelatch(QScriptContext *ctx, QScriptEngine *engine);

private:
    QScriptEngine *m_engine;
    QString m_code;
};

#endif // CLASSSCRIPT_H
