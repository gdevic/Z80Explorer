#include "ClassWatch.h"

#include <QFile>
#include <QDataStream>

ClassWatch::ClassWatch(QObject *parent) : QObject(parent)
{
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
                in >> w.name >> w.x >> w.y >> w.n >> num;
                if (num)
                {
                    while (num--)
                    {
                        net_t n;
                        in >> n;
                        w.nn.append(n);
                    }
                }
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
        {
            out << w.name << w.x << w.y << w.n << w.nn.count();
            for (auto n : w.nn)
                out << n;
        }
    }
    catch(...) { return false; }
    return true;
}
