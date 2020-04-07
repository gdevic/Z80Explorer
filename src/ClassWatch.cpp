#include "ClassWatch.h"
#include "ClassController.h"

// Serialization support
#include "cereal/archives/binary.hpp"
#include "cereal/types/QString.hpp"
#include "cereal/types/QVector.hpp"
#include <fstream>

#include <QDebug>
#include <QFile>

ClassWatch::ClassWatch()
{
    doReset();
}

/*
 * Adds net watch data to the watch item's buffer
 */
void ClassWatch::append(watch *w, uint hcycle, net_t value)
{
    hringstart = (hcycle > MAX_WATCH_HISTORY) ? (hcycle - MAX_WATCH_HISTORY) : 0;
    next = hcycle % MAX_WATCH_HISTORY;
    w->d[next] = value;
    next = (next + 1) % MAX_WATCH_HISTORY;
}

/*
 * Returns watch data at the specified cycle position. The watch structure can represent a net
 * or a bus. For nets, we simply return the net_t bit stored for that net.
 * For a bus, we need to aggregate all nets that comprise this bus.
 * This function variation returns a net.
 */
net_t ClassWatch::at(watch *w, uint hcycle)
{
    if ((hringstart == 0) && (hcycle >= next))
        return 3;
    if ((hcycle < hringstart) || (hcycle >= (hringstart + MAX_WATCH_HISTORY)))
        return 3;
    if (w->n) // n is non-zero: it is a net
        return w->d[hcycle % MAX_WATCH_HISTORY];
    return 4; // The watch is a bus error
}

/*
 * Returns watch data at the specified cycle position. The watch structure can represent a net
 * or a bus. For nets, we simply return the net_t bit stored for that net.
 * For a bus, we need to aggregate all nets that comprise this bus.
 * This function variation returns a bus, ok is set to true if the bus, and all its nets, are valid.
 */
uint ClassWatch::at(watch *w, uint hcycle, bool &ok)
{
    ok = false;
    if ((hringstart == 0) && (hcycle >= next))
        return 0;
    if ((hcycle < hringstart) || (hcycle >= (hringstart + MAX_WATCH_HISTORY)))
        return 0;
    if (w->n) // n is non-zero: it is a net
        return 0;
    uint value = 0; // The watch is a bus
    const QVector<net_t> &nets = ::controller.getNetlist().getBus(w->name);
    uint width = nets.count() - 1; // Buses are defined from LSB to MSB; we are filling in bits from MSB
    for (auto n : nets)
    {
        value >>= 1;
        watch *wb = find(n);
        if (wb == nullptr)
            return 0;
        value |= uint(wb->d[hcycle % MAX_WATCH_HISTORY]) << width;
    }
    return value;
}

/*
 * Returns the watch of a given name or nullptr
 */
watch *ClassWatch::find(QString name)
{
    for (auto &item : m_watchlist)
        if (item.name == name)
            return &item;
    return nullptr;
}

/*
 * Returns the watch containing the given net number
 * XXX make m_watchlist a hash
 */
watch *ClassWatch::find(net_t net)
{
    for (auto &item : m_watchlist)
        if (item.n == net)
            return &item;
    return nullptr;
}

void ClassWatch::doReset()
{
    hringstart = 0;
    next = 0;
    for (auto &watch : m_watchlist)
        memset(watch.d, 3, sizeof(watch.d));
}

/*
 * Returns the list of net and bus names in the watchlist
 */
QStringList ClassWatch::getWatchlist()
{
    QStringList list;
    for (auto item : m_watchlist)
        list.append(item.name);
    return list;
}

/*
 * Sets new watchlist
 * XXX updateWatchlist ?
 */
void ClassWatch::setWatchlist(QStringList list)
{
    QVector<watch> newlist;
    for (auto item : list)
    {
        watch *w = find(item);
        if (w)
            newlist.append(*w);
        else
        {
            watch w {};
            w.name = item;
            newlist.append(w);
        }
    }
    m_watchlist = newlist;
}

/*
 * Loads a watchlist
 */
bool ClassWatch::loadWatchlist(QString name)
{
    try
    {
        std::ifstream os(name.toLatin1(), std::ios::binary);
        cereal::BinaryInputArchive archive(os);
        archive(m_watchlist);
    }
    catch(...) { qWarning() << "Unable to load" << name; }
    return true;
}

/*
 * Saves the current watchlist
 */
bool ClassWatch::saveWatchlist(QString name)
{
    try
    {
        std::ofstream os(name.toLatin1(), std::ios::binary);
        cereal::BinaryOutputArchive archive(os);
        archive(m_watchlist);
    }
    catch(...) { qWarning() << "Unable to save" << name; }
    return true;
}
