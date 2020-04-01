#include "ClassWatch.h"

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
    try
    {
        QFile file(name);
        file.open(QIODevice::ReadOnly);
        QDataStream in(&file);
        uint num;
        in >> num;
        if (num) // Don't load an empty watchlist
        {
            m_watchlist.clear();
            while (num--)
            {
                watch w {};
                in >> w.name >> w.x >> w.y >> w.n;
                m_watchlist.append(w);
            }
        }
    }
    catch(...) { return false; }
    return true;
}

/*
 * Saves the current watchlist
 */
bool ClassWatch::saveWatchlist(QString name)
{
    try
    {
        QFile file(name);
        file.open(QIODevice::WriteOnly);
        QDataStream out(&file);
        out << m_watchlist.count();
        for (auto w : m_watchlist)
            out << w.name << w.x << w.y << w.n;
    }
    catch(...) { return false; }
    return true;
}
