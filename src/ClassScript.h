#ifndef CLASSSCRIPT_H
#define CLASSSCRIPT_H

#include <QJSEngine>

/*
 * This class provides scripting functionality to the app
 */
class ClassScript : public QObject
{
    Q_OBJECT                        //* <- Methods of the scripting object "script" below
public:
    Q_INVOKABLE explicit ClassScript(QObject *parent = nullptr);
    void init(QJSEngine *sc);

signals:
    void response(QString);         //* Write a response string to the command list

public slots:
    void stopx();                   // Stops any running script evaluation
    void exec(QString cmd);         //* Evaluates and runs commands

public:
    Q_INVOKABLE QJSValue load(QString fileName);
    Q_INVOKABLE QJSValue help();
    Q_INVOKABLE QJSValue run(uint cycles = 0);
    Q_INVOKABLE QJSValue stop();
    Q_INVOKABLE QJSValue reset();
    Q_INVOKABLE QJSValue n(QVariant net);
    Q_INVOKABLE QJSValue t(uint n);
    Q_INVOKABLE QJSValue ex(uint n);
    Q_INVOKABLE QJSValue relatch();

private:
    QJSEngine *m_engine;
};

#endif // CLASSSCRIPT_H
