#ifndef CLASSWATCH_H
#define CLASSWATCH_H

#include <QObject>

typedef uint16_t net_t;                 // Type of an index to the net array
typedef uint8_t  pin_t;                 // Type of the pin state (0, 1; or 2 for floating)

// Watch structure defines a net to watch. A net is a single object identified by net number "n"
struct watch
{
    QString name;                       // The name of the net to watch
    uint x, y;                          // Coordinates of a pushpin on the chip image
    net_t n;                            // Net number (if nonzero)

    template <class Archive> void serialize(Archive & ar) { ar(name, x, y, n); }
};

/*
 * ClassWatch holds the watchlist, the list of nets whose state is tracked during the simulation
 */
class ClassWatch : public QObject
{
    Q_OBJECT
public:
    ClassWatch() {};

    bool loadWatchlist(QString name);   // Loads a watchlist
    bool saveWatchlist(QString name);   // Saves the current watchlist
    QStringList getWatchlist();         // Returns the list of net names in the watchlist
    void setWatchlist(QStringList);     // Sets new watchlist
    watch *find(QString name);          // Returns the watch of a given name or nullptr

private:
    QVector<watch> m_watchlist;         // The list of items that are tracked
};

#endif // CLASSWATCH_H
