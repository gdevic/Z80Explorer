#ifndef CLASSNETLIST_H
#define CLASSNETLIST_H

#include "AppTypes.h"
#include <QObject>
#include <QHash>

// Contains individual transistor definition
struct trans
{
    tran_t id;                          // Transistor number
    net_t gate;                         // Net connected to its gate
    net_t c1, c2;                       // Connections 1, 2 (source, drain) nets
    bool on {false};                    // Is the transistor on?
};

// Contains netlist net definition: net is a trace with equal potential and it connects a number of transistors
struct net
{
    QVector<trans *> gates;             // The list of transistors for which this net is a gate
    QVector<trans *> c1c2s;             // The list of transistors for which this net is either a source or a drain
    bool state {false};                 // The voltage on the net is high (if not floating)
    bool floats {false};                // Net can float (used with ab, db, mreq, iorq, rd, wr to read hi-Z state)
    bool pullup {false};                // Net is being pulled-up
    bool pulldown {false};              // Net is being pulled-down
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
    const QVector<net_t> &getBus(QString &name) // Returns nets that comprise a bus
        { static const QVector<net_t>x {}; return m_buses.contains(name) ? m_buses[name] : x; }
    inline net_t get(const QString &name)       // Returns net number given its name
        { return m_netnums.contains(name) ? m_netnums[name] : 0; }
    inline const QString &get(net_t n)          // Returns net name given its number
        { return m_netnames[n]; }
    const QStringList get(const QVector<net_t> &nets); // Returns net names for each net in the list

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

    QString cmdNet(net_t net);                  // Implements script command net(n)
    QString cmdTrans(tran_t t);                 // Implements script command trans(t)

protected:
    QVector<trans> m_transdefs;                 // Array of transistors, indexed by the transistor number
    QVector<net> m_netlist;                     // Array of nets, indexed by the net number
    net_t ngnd {}, npwr {};                     // 'vss' and 'vcc' nets (expected values: 1 and 2)

    uint8_t readByte(const QString &name);      // Returns a byte value read from the netlist for a particular net bus
    pin_t readBit(const QString &name);         // Returns a bit value read from the netlist for a particular net
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
};

#endif // CLASSNETLIST_H
