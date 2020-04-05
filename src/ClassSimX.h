#ifndef CLASSSIMX_H
#define CLASSSIMX_H

#include "z80state.h"
#include <QHash>
#include <QObject>
#include <QTime>
#include <QTimer>

// Contains individual transistor definition
class trans
{
    net_t gate;                         // Gate net
    net_t c1, c2;                       // Connections 1, 2 (source, drain) nets
    bool on {false};                    // Is the transistor on?
    friend class ClassSimX;
};

// Contains netlist net definition: net is a trace with equal potential and it connects a number of transistors
class net
{
    QVector<trans *> gates;             // The list of transistors for which this net is a gate
    QVector<trans *> c1c2s;             // The list of transistors for which this net is either a source or a drain
    bool state {false};                 // The voltage on the net is high (if not floating)
    bool floats {true};                 // Net is in the floating state (neither pulled up nor pulled down)
    bool pullup {false};                // Net is being pulled-up
    bool pulldown {false};              // Net is being pulled-down
    friend class ClassSimX;
};

/*
 * ClassSimX implements a chip netlist simulator
 */
class ClassSimX : public QObject
{
    Q_OBJECT
public:
    explicit ClassSimX();
    bool loadResources(QString dir);
    void initChip();                    // One-time initialization
    net_t getNetlistCount()             // Returns the number of nets in the netlist
        { return m_netlist.count(); }
    bool getNetState(net_t i)           // Returns the net logic state
        { return m_netlist[i].state; }
    void readState(z80state &z);        // Reads chip state into a state structure

signals:
    void runStopped();                  // Current simulation run completed

public slots:
    void doReset();                     // Run chip reset sequence
    void doRunsim(uint ticks);          // Controls the simulation

private slots:
    void onTimeout();                   // Dump z80 state every 500ms when running the simulation

private:
    bool loadNodenames(QString dir);
    bool loadTransdefs(QString dir);
    bool loadPullups(QString dir);

    void handleMemRead(uint16_t ab);    // Simulated chip requested memory read
    void handleMemWrite(uint16_t ab);   // Simulated chip requested memory write
    void handleIORead(uint16_t ab);     // Simulated chip requested IO read
    void handleIOWrite(uint16_t ab);    // Simulated chip requested IO write
    void handleIrq(uint16_t ab);        // Simulated chip requested interrupt service (to read data on the bus)

    void setDB(uint8_t db);             // Sets data bus to a value
    void set(bool on, QString name);    // Sets a named input net to pullup or pulldown status
    uint readByte(QString name);        // Returns a byte value read from the netlist for a particular net bus
    uint readBit(QString name);         // Returns a bit value read from the netlist for a particular net
    pin_t readPin(QString name);        // Returns the pin value
    uint16_t readAB();                  // Returns the value on the address bus

    void halfCycle();
    bool getNetValue();
    void recalcNetlist(QVector<net_t> list);
    void setTransOn(class trans &t);
    void setTransOff(class trans &t);
    void addRecalcNet(net_t n);
    void recalcNet(net_t n);
    QVector<net_t> allNets();
    void getNetGroup(net_t n);
    void addNetToGroup(net_t n);

    QVector<trans> m_transdefs;         // Array of transistors, indexed by the transistor number
    QVector<net> m_netlist;             // Array of nets, indexed by the net number

    QVector<net_t> recalcList;
    QVector<net_t> group;
    net_t ngnd {}, npwr {};             // 'vss' and 'vcc' nets (expected values: 1 and 2)

    QTime m_time;                       // Calculates elapsed time during a simulation thread run
    QTimer m_timer;                     // Timer to dump z80 state every 500ms when running the simulation
    QAtomicInt m_runcount {};           // Simulation thread down-counts this to exit
    QAtomicInt m_hcyclecnt {};          // Simulation half-cycle count (resets on each runstart event)
    QAtomicInt m_hcycletotal {};        // Total simulation half-cycle count (resets on a chip reset)
};

#endif // CLASSSIMX_H
