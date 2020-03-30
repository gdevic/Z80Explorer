#ifndef CLASSWATCH_H
#define CLASSWATCH_H

#include <QObject>

typedef uint16_t net_t;                 // Type of an index to the net array
typedef uint8_t  pin_t;                 // Type of the pin state (0, 1; or 2 for floating)

// Watch structure defines a net or a bus to watch. A net is a single object identified by net number "n"
// A bus is a collection of nets; n is zero and nn contains a list of nets comprising a bus.
struct watch
{
    QString name;                       // The name of the net or bus to watch
    uint x, y;                          // Coordinates of a pushpin on the chip image
    net_t n;                            // Net number (if nonzero)
    QVector<net_t> nn;                  // List of nets comprising the bus
};

/*
 * ClassWatch holds the watchlist, the list of nets whose state is tracked during the simulation
 */
class ClassWatch : public QObject
{
    Q_OBJECT
public:
    explicit ClassWatch(QObject *parent);
    bool loadWatchlist(QString name);   // Loads a watchlist
    bool saveWatchlist(QString name);   // Saves the current watchlist

private:
    QList<watch> m_watchlist;           // The list of items that are tracked
};

#endif // CLASSWATCH_H
