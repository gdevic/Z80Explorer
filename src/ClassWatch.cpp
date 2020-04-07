#include "ClassWatch.h"

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
 * Adds watch data to the watch item's buffer
 */
void ClassWatch::append(watch *w, uint hcycle, net_t value)
{
    hringstart = (hcycle > MAX_WATCH_HISTORY) ? (hcycle - MAX_WATCH_HISTORY) : 0;
    next = hcycle % MAX_WATCH_HISTORY;
    w->d[next] = value;
    next = (next + 1) % MAX_WATCH_HISTORY;
}

net_t ClassWatch::at(watch *w, uint hcycle)
{
    if ((hringstart == 0) && (hcycle >= next))
        return 3;
    if ((hcycle < hringstart) || (hcycle >= (hringstart + MAX_WATCH_HISTORY)))
        return 3;
    return w->d[hcycle % MAX_WATCH_HISTORY];
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

void ClassWatch::doReset()
{
    hringstart = 0;
    next = 0;
    for (auto &watch : m_watchlist)
        memset(watch.d, 3, sizeof(watch.d));
}

/*
 * Returns the list of net names in the watchlist
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
