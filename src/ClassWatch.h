#ifndef CLASSWATCH_H
#define CLASSWATCH_H

#include "z80state.h"

#define MAX_WATCH_HISTORY  1000

/*
 * Watch structure defines a net to watch. A net is a single object identified by a net number "n".
 */
struct watch
{
    QString name;                       // The name of the net to watch
    net_t n;                            // Net number (if nonzero)

    template <class Archive> void serialize(Archive & ar) { ar(name, n); }

    net_t d[MAX_WATCH_HISTORY];         // Circular buffer of watch data (not serialized out)
};

/*
 * ClassWatch holds the watchlist, the list of nets whose state is tracked during the simulation
 */
class ClassWatch : public QObject
{
    Q_OBJECT
public:
    ClassWatch();

    bool loadWatchlist(QString name);   // Loads a watchlist
    bool saveWatchlist(QString name);   // Saves the current watchlist
    QStringList getWatchlist();         // Returns the list of net names in the watchlist
    void setWatchlist(QStringList);     // Sets new watchlist
    watch *find(QString name);          // Returns the watch of a given name or nullptr
    void doReset();                     // Chip reset sequence, reset watch history buffers

    inline watch *getFirst(int &it)     // Iterator
        { it = 1; return (m_watchlist.count() > 0) ? m_watchlist.data() : nullptr; }
    inline watch *getNext(int &it)      // Iterator
        { return (it < m_watchlist.count()) ? &m_watchlist[it++] : nullptr; }

    void append(watch *w, uint hcycle, net_t value); // Adds watch data to the specified cycle position
    net_t at(watch *w, uint hcycle);    // Returns watch data at the specified cycle position

    uint gethstart() { return hringstart; }

private:
    QVector<watch> m_watchlist;         // The list of watch items that are tracked
    uint next;                          // Next index within each watch buffer to write to
    uint hringstart;                    // Buffer start maps to this absolute cycle

    friend class WidgetWaveform;        // This is ok: the whole purpose of that widget is to draw the data contained herein
};

#endif // CLASSWATCH_H
