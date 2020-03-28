#ifndef CLASSSIMX_H
#define CLASSSIMX_H

#include <QObject>

/*
 * ClassSimX implements a chip netlist simulator
 */
class ClassSimX : public QObject
{
    Q_OBJECT
public:
    explicit ClassSimX(QObject *parent);

signals:

};

#endif // CLASSSIMX_H
