#ifndef CLASSSIMZ80_H
#define CLASSSIMZ80_H

#include "ClassNetlist.h"
#include "z80state.h"
#include <QAtomicInteger>
#include <QElapsedTimer>
#include <QTimer>

#define USE_MY_LISTS 1
#define EARLY_LOOP_DETECTION 1

/*
 * ClassSimZ80 implements Z80 chip netlist simulator
 */
class ClassSimZ80 : public QObject, public ClassNetlist
{
    Q_OBJECT
public:
    explicit ClassSimZ80();
    bool initChip();                    // One-time chip initialization
    void readState(z80state &z);        // Reads chip state into a state structure
    uint doReset();                     // Run chip reset sequence
    void doRunsim(uint ticks);          // Run the simulation for the given number of clocks
    bool setPin(uint index, pin_t p);   // Sets an input pin to a value
    bool isRunning() { return m_runcount; }; // Returns true if the simulation is currently running
    uint16_t getPC()                    // Returns the current value of the PC register
        { return (readByte("reg_pch") << 8) | readByte("reg_pcl"); }
    uint getCurrentHCycle() { return m_hcycletotal; }
    uint getEstHz() { return m_estHz; }

public slots:
    void onShutdown()                   // Called when the app is closing
        { doRunsim(0); ClassNetlist::onShutdown(); } // Stop the running sim and pass on the signal

private slots:
    void onTimeout();                   // Dump z80 state every 500ms when running the simulation

private:
    void handleMemRead(uint16_t ab);    // Simulated chip requested memory read
    void handleMemWrite(uint16_t ab);   // Simulated chip requested memory write
    void handleIORead(uint16_t ab);     // Simulated chip requested IO read
    void handleIOWrite(uint16_t ab);    // Simulated chip requested IO write
    void handleIrq();                   // Simulated chip requested interrupt service (to read data on the bus)

    void setDB(uint8_t db);             // Sets data bus to a value
    void set(bool on, QString name);    // Sets a named input net to pullup or pulldown status

    //----------------------- Simulator ------------------------
    void halfCycle();
    bool getNetValue();
    void setTransOn(struct Trans *t);
    void setTransOff(struct Trans *t);
    void addRecalcNet(net_t n);
    void recalcNet(net_t n);
    void getNetGroup(net_t n);
    void addNetToGroup(net_t n);
    int t01opt;                         // Early loop detection (performance optimization)
#if USE_MY_LISTS
    void recalcNetlist();
    void allNets();

    net_t m_recalcList[MAX_NETS];
    int m_recalcListIndex {0};
    net_t m_list[MAX_NETS];
    int m_listIndex {0};
    net_t m_group[MAX_NETS];
    int m_groupIndex {0};
#else
    QVector<net_t> allNets();
    void recalcNetlist(QVector<net_t> &list);

    QVector<net_t> recalcList;
    QVector<net_t> group;
#endif
    //----------------------------------------------------------
    QTimer m_timer;                     // Simulation heartbeat timer (500 ms)
    uint m_estHz {};                    // Estimated simulated frequency
    QElapsedTimer m_elapsed;            // Calculates elapsed time during a simulation thread run
    QAtomicInt m_runcount {};           // Simulation thread down-counts this to exit
    QAtomicInt m_hcyclecnt {};          // Simulation half-cycle count (resets on each runstart event)
    QAtomicInt m_hcycletotal {};        // Total simulation half-cycle count (resets on a chip reset)
};

#endif // CLASSSIMZ80_H
