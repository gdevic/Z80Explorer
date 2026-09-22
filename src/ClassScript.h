#ifndef CLASSSCRIPT_H
#define CLASSSCRIPT_H

#include <QJSEngine>
#include <QStringList>

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
    Q_INVOKABLE QJSValue eqTree(QVariant n);
    Q_INVOKABLE QJSValue netDrivers(QVariant n);

    // Net value reads for instrumentation scripts
    Q_INVOKABLE int     readBit(const QString &name);            // Returns 0, 1, or 2 (hi-Z); -1 if net not found
    Q_INVOKABLE int     readByte(const QString &base);           // Reads <base>0..<base>7 as an 8-bit value
    Q_INVOKABLE QString readBits(const QStringList &names);      // Returns a CSV of current values, '-' for unknown names
    Q_INVOKABLE QString getMTState();                            // Returns e.g. "M1T2" by scanning m1..m6 / t1..t6 latches
    Q_INVOKABLE bool    saveText(const QString &path, const QString &content); // Writes content to a text file
    Q_INVOKABLE void    setNetName(const QString &name, uint net); // Assigns a name to a net number (persists via save())
    Q_INVOKABLE void    renameNet(const QString &name, uint net);  // Renames an already-named net; refuses if net has no existing name
    Q_INVOKABLE void    deleteNetName(uint net);                   // Clears the name of a named net

    // Writing user data to disk. These do not disturb the session: a simulation in progress keeps running
    Q_INVOKABLE QJSValue save(const QString &id = {});            // Saves one item by id, or every available item
    Q_INVOKABLE QJSValue saveList();                              // Lists every save item: id, name, files, available
    Q_INVOKABLE bool     saveNetnames();                          // Net names, buses and comments; same as save("netnames")

private:
    QJSEngine *m_engine {};
};

#endif // CLASSSCRIPT_H
