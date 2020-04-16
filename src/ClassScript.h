#ifndef CLASSSCRIPT_H
#define CLASSSCRIPT_H

#include <QObject>

/*
 * This class provides scripting functionality to the app
 */
class ClassScript : public QObject
{
    Q_OBJECT
public:
    explicit ClassScript(QObject *parent = nullptr);

signals:
    void response(QString);     // Write a response string to the command list

public slots:
    void run(QString cmd);      // Evaluates and runs command
};

#endif // CLASSSCRIPT_H
