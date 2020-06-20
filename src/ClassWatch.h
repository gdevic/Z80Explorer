#ifndef CLASSWATCH_H
#define CLASSWATCH_H

#include "AppTypes.h"
#include <QObject>

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
    pin_t d[MAX_WATCH_HISTORY];         // Circular buffer of watch data (not serialized out)

    watch(QString _name, net_t net): name(_name), n(net) { clear(); }
    void clear() { memset(d, 3, sizeof(d)); }
};

/*
 * ClassWatch holds the watchlist - the list of nets and buses whose state is tracked during the simulation,
 * the simulation data (in circular buffers) for those signals and the code to query and manipulate this data.
 */
class ClassWatch : public QObject
{
    Q_OBJECT
public:
    ClassWatch();

    QStringList getWatchlist();         // Returns a list of net and bus names in the watchlist
    void updateWatchlist(QStringList);  // Updates watchlist using a new list of watch names
    void clear();                       // Clear watch history buffers (used on simulation reset)

    inline watch *getFirst(int &it)     // Iterator
        { it = 1; return (m_watchlist.count() > 0) ? m_watchlist.data() : nullptr; }
    inline watch *getNext(int &it)      // Iterator
        { return (it < m_watchlist.count()) ? &m_watchlist[it++] : nullptr; }

    watch *find(QString name);          // Returns a watch of a given name or nullptr
    void append(watch *w, uint hcycle, pin_t value); // Adds net watch data to the specified cycle position
    pin_t at(watch *w, uint hcycle);    // Returns net watch data at the specified cycle position
    uint at(watch *w, uint hcycle, uint &ok); // Returns bus watch data at the specified cycle position
    uint gethstart() { return m_hring_start; } // Returns the absolute hcycle of the start of our buffers

    bool load(QString fileName);        // Loads a watchlist
    bool save(QString fileName);        // Saves a watchlist

public slots:
    void onShutdown();                  // Called when the app is closing
    void onNetName(Netop op, const QString name, const net_t); // Handles events related to managing the net names

private:
    watch *find(net_t net);             // Returns a watch containing a given net number or nullptr

    QVector<watch> m_watchlist;         // The list of watch items that are tracked
    QString m_jsonFile;                 // File name used to load watchlist
    uint m_hcycle_last {};              // Last cycle number for which we got data stored
    uint m_hring_start {};              // Buffer start maps to this absolute cycle
};

#endif // CLASSWATCH_H
