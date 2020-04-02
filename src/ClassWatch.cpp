#include "ClassWatch.h"

// Serialization support
#include "cereal/archives/binary.hpp"
#include "cereal/types/QString.hpp"
#include "cereal/types/QVector.hpp"
#include <fstream>

#include <QDataStream>
#include <QFile>

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
    std::ifstream os(name.toLatin1(), std::ios::binary);
    cereal::BinaryInputArchive archive(os);
    archive(m_watchlist);

    return true;
}

/*
 * Saves the current watchlist
 */
bool ClassWatch::saveWatchlist(QString name)
{
    std::ofstream os(name.toLatin1(), std::ios::binary);
    cereal::BinaryOutputArchive archive(os);
    archive(m_watchlist);

    return true;
}
