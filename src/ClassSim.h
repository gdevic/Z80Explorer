#ifndef CLASSSIM_H
#define CLASSSIM_H

#include <QObject>

/*
 * ClassSim contains functions to control simulation and to interface with the
 * legacy C++ simulator code
 */
class ClassSim : public QObject
{
    Q_OBJECT
public:
    ClassSim() {};
    bool loadSimResources(QString dir); // Loads sim resources (netlist)

public slots:
    void onRun();                       // Run Z80 netlist simulator
    void onStop();                      // Stop running Z80 netlist simulator

};

#endif // CLASSSIM_H
