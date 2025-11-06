#ifndef CLASSSCRIPT_H
#define CLASSSCRIPT_H

#include <QJSEngine>

/*
 * This class provides scripting functionality to the app
 */
class ClassScript : public QObject
{
    Q_OBJECT
public:
    explicit ClassScript(QObject *parent = nullptr);
    void init(QJSEngine *sc);

signals:
    Q_INVOKABLE void print(QString);// Write out a string to the command list (connected from DockCommand)
    Q_INVOKABLE void save();        // Saves all changes to all custom and config files (connected from ClassController)

public slots:
    void exec(QString cmd, bool echo = true); // Evaluates and runs commands

public:
    Q_INVOKABLE void load(QString fileName = {});
    Q_INVOKABLE void run(uint hcycles);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void t(uint n);
    Q_INVOKABLE void n(QVariant net);
    Q_INVOKABLE void eq(QVariant n);
    Q_INVOKABLE void relatch();
    Q_INVOKABLE void ex(uint n);
    Q_INVOKABLE QJSValue execApp(const QString &path, const QStringList &args, bool synchronous = true);

private:
    QJSEngine *m_engine {};
};

#endif // CLASSSCRIPT_H
