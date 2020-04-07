#ifndef CLASSWATCH_H
#define CLASSWATCH_H

#include "z80state.h"

#define MAX_WATCH_HISTORY  1000

/*
 * Watch structure defines a net or a bus to watch. A net is a single object identified by a net number "n".
 * A bus is a collection of nets, each of which needs to be listed and watched, the bus simply aggregates their data.
 * When a watch structure holds a bus, net number is 0 and the list of buses needs to be fetched from the netlist.
 */
struct watch
{
    QString name;                       // The name of the net to watch
    net_t n;                            // Net number (if nonzero), if zero, it is a bus

    template <class Archive> void serialize(Archive & ar) { ar(name, n); }

    net_t d[MAX_WATCH_HISTORY];         // Circular buffer of watch data (not serialized out)
};

/*
 * ClassWatch holds the watchlist, the list of nets and buses whose state is tracked during the simulation
 */
class ClassWatch : public QObject
{
    Q_OBJECT
public:
    ClassWatch();

    bool loadWatchlist(QString name);   // Loads a watchlist
    bool saveWatchlist(QString name);   // Saves the current watchlist
    QStringList getWatchlist();         // Returns the list of net and bus names in the watchlist
    void updateWatchlist(QStringList);  // Updates watchlist using a new list of watch names
    void clear();                       // Clear watch history buffers (used on simulation reset)

    inline watch *getFirst(int &it)     // Iterator
        { it = 1; return (m_watchlist.count() > 0) ? m_watchlist.data() : nullptr; }
    inline watch *getNext(int &it)      // Iterator
        { return (it < m_watchlist.count()) ? &m_watchlist[it++] : nullptr; }

    watch *find(QString name);          // Returns the watch of a given name or nullptr
    void append(watch *w, uint hcycle, net_t value); // Adds net watch data to the specified cycle position
    bool is_bus(watch *w)               // Returns true if the watch contains a bus (as opposed to a net)
        { return !w->n; }
    net_t at(watch *w, uint hcycle);    // Returns net watch data at the specified cycle position
    uint at(watch *w, uint hcycle, bool &ok); // Returns bus watch data at the specified cycle position
    uint gethstart() { return hringstart; } // Returns the absolute hcycle of the start of our buffers

private:
    watch *find(net_t net);             // Returns a watch containing a given net number or nullptr

    QVector<watch> m_watchlist;         // The list of watch items that are tracked
    uint next;                          // Next index within each watch buffer to write to
    uint hringstart;                    // Buffer start maps to this absolute cycle

    friend class WidgetWaveform;        // This is ok: the whole purpose of that widget is to draw the data contained herein
};

#endif // CLASSWATCH_H
