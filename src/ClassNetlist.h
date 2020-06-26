#ifndef CLASSNETLIST_H
#define CLASSNETLIST_H

#include "AppTypes.h"
#include "ClassLogic.h"
#include <QObject>
#include <QHash>

// Contains individual transistor definition
struct Trans
{
    tran_t id;                          // Transistor number
    net_t gate;                         // Net connected to its gate
    net_t c1, c2;                       // Connections 1, 2 (source, drain) nets
    bool on {false};                    // Is the transistor on?
};

// Contains netlist net definition: net is a trace with equal potential and it connects a number of transistors
struct Net
{
    QVector<Trans *> gates;             // The list of transistors for which this net is a gate
    QVector<Trans *> c1c2s;             // The list of transistors for which this net is either a source or a drain
    bool state {false};                 // The voltage on the net is high (if not floating)
    bool floats {false};                // Net can float (used with ab, db, mreq, iorq, rd, wr to read hi-Z state)
    bool isHigh {false};                // Net is being pulled high
    bool isLow {false};                 // Net is being pulled low
    bool hasPullup {false};             // Net has a (permanent) pull-up resistor
};

/*
 * This class represents netlist data and operations on it
 */
class ClassNetlist
{
public:
    ClassNetlist();

    bool loadResources(const QString dir);
    QStringList getNetnames();                  // Returns a list of net and bus names concatenated
    inline net_t get(const QString &name)       // Returns net number given its name
        { return m_netnums.contains(name) ? m_netnums[name] : 0; }
    inline const QString &get(net_t n)          // Returns net name given its number
        { return m_netnames[n]; }
    const QStringList get(const QVector<net_t> &nets); // Returns sorted net names for each net on the list
    const QVector<net_t> &getBus(QString &name) // Returns nets that comprise a bus
        { static const QVector<net_t>x {}; return m_buses.contains(name) ? m_buses[name] : x; }
    bool getTnet(tran_t t, net_t &c1, net_t &c2); // Returns a transistor's source and drain connections

    const QVector<net_t> netsDriving(net_t n);  // Returns a sorted list of nets that the given net is driving
    const QVector<net_t> netsDriven(net_t n);   // Returns a sorted list of nets that the given net is being driven by

    uint getNetlistCount()                      // Returns the number of nets in the netlist
        { return m_netlist.count(); }
    bool getNetState(net_t i)                   // Returns the net logic state
        { return m_netlist[i].state; }
    pin_t getNetStateEx(net_t n);               // Returns the net extended logic state (including hi-Z)

    void addBus(const QString &name, const QStringList &netlist); // Adds bus by name and a set of nets listed by their name
    void clearBuses() { m_buses.clear(); }      // Clear all buses, used only by the DialogEditBuses

    // Do not call these functions directly; call the ::controller functional counterparts
    void eventNetName(Netop op, const QString name, const net_t);
    void onShutdown();                          // Called when the app is closing

    const QString netInfo(net_t net);           // Returns basic net information as string
    const QString transInfo(tran_t t);          // Returns basic transistor information as string
    Logic *getLogicTree(net_t net);             // Returns the bipartite tree describing the logic connections of a net
    void optimizeLogicTree(Logic **llr);        // Optimizes, in place, logic tree by coalescing suitable nodes
    QString equation(net_t net);                // Returns a string describing the logic connections of a net

protected:
    QVector<Trans> m_transdefs;                 // Array of transistors, indexed by the transistor number
    QVector<Net> m_netlist;                     // Array of nets, indexed by the net number
    net_t ngnd {}, npwr {}, nclk {};            // 'vss', 'vcc' and 'clk' nets (expected values: 1, 2 and 3)

    uint8_t readByte(const QString &name);      // Returns a byte value read from the netlist for a particular net bus
    pin_t readBit(const QString &name);         // Returns a bit value read from the netlist for a particular net, by net name
    pin_t readBit(const net_t n);               // Returns a bit value read from the netlist for a particular net, by net number
    uint16_t readAB();                          // Returns the value on the address bus

private:
    bool loadNetNames(const QString fileName, bool);
    bool loadTransdefs(const QString dir);
    bool loadPullups(const QString dir);
    bool saveNetNames(const QString fileName);

    // The lookup between net names and their numbers is performance critical, so we keep two ways to access them:
    QString m_netnames[MAX_NETS] {};            // List of net names, directly indexed by the net number
    QHash<QString, net_t> m_netnums {};         // Hash of net names to their net numbers; key is the net name string
    bool m_netoverrides[MAX_NETS] {};           // Net names that are overriden or new
    QHash<QString, QVector<net_t>> m_buses {};  // Hash of bus names to their list (vector) of nets

    // Generates a logic equation driving a net
    void parse(Logic *node);                    // Recursive parse of the netlist starting with the given node
};

#endif // CLASSNETLIST_H
