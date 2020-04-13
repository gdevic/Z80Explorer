#ifndef CLASSSIMX_H
#define CLASSSIMX_H

#include "ClassNetlist.h"
#include <QElapsedTimer>
#include <QTimer>

/*
 * ClassSimX implements a chip netlist simulator
 */
class ClassSimX : public QObject, public ClassNetlist
{
    Q_OBJECT
public:
    explicit ClassSimX();
    void initChip();                    // One-time initialization
    void readState(z80state &z);        // Reads chip state into a state structure

signals:
    void runStopped(uint hcycle);       // Current simulation run completed at a given half-cycle

public slots:
    void doReset();                     // Run chip reset sequence
    void doRunsim(uint ticks);          // Controls the simulation

private slots:
    void onTimeout();                   // Dump z80 state every 500ms when running the simulation

private:
    void handleMemRead(uint16_t ab);    // Simulated chip requested memory read
    void handleMemWrite(uint16_t ab);   // Simulated chip requested memory write
    void handleIORead(uint16_t ab);     // Simulated chip requested IO read
    void handleIOWrite(uint16_t ab);    // Simulated chip requested IO write
    void handleIrq(uint16_t ab);        // Simulated chip requested interrupt service (to read data on the bus)

    void setDB(uint8_t db);             // Sets data bus to a value
    void set(bool on, QString name);    // Sets a named input net to pullup or pulldown status

    //----------------------- Simulator ------------------------
    void halfCycle();
    bool getNetValue();
    void recalcNetlist(QVector<net_t> list);
    void setTransOn(struct trans &t);
    void setTransOff(struct trans &t);
    void addRecalcNet(net_t n);
    void recalcNet(net_t n);
    QVector<net_t> allNets();
    void getNetGroup(net_t n);
    void addNetToGroup(net_t n);
    QVector<net_t> recalcList;
    QVector<net_t> group;
    //----------------------------------------------------------

    QTimer m_timer;                     // Timer to dump z80 state every 500ms when running the simulation
    QElapsedTimer m_elapsed;            // Calculates elapsed time during a simulation thread run
    QAtomicInt m_runcount {};           // Simulation thread down-counts this to exit
    QAtomicInt m_hcyclecnt {};          // Simulation half-cycle count (resets on each runstart event)
    QAtomicInt m_hcycletotal {};        // Total simulation half-cycle count (resets on a chip reset)
};

#endif // CLASSSIMX_H
