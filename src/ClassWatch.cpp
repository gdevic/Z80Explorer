#include "ClassWatch.h"
#include "ClassController.h"
#include <QDebug>
#include <QFile>

ClassWatch::ClassWatch()
{
    clear();
}

void ClassWatch::onShutdown()
{
    QSettings settings;
    QString path = settings.value("ResourceDir").toString();
    Q_ASSERT(!path.isEmpty());
    save(path);
}

/*
 * Adds net watch data to the watch item's buffer at a specified cycle time
 */
void ClassWatch::append(watch *w, uint hcycle, net_t value)
{
    m_hcycle_last = hcycle + 1;
    uint i = hcycle % MAX_WATCH_HISTORY;
    w->d[i] = value;

    if (hcycle >= MAX_WATCH_HISTORY)
        m_hring_start = hcycle - MAX_WATCH_HISTORY + 1;
    else
        m_hring_start = 0;
}

/*
 * Returns watch data at the specified cycle position. The watch structure can represent a net
 * or a bus. For nets, we simply return the net_t bit stored for that net.
 * For a bus, we need to aggregate all nets that comprise this bus.
 *
 * This function variation returns a net.
 */
net_t ClassWatch::at(watch *w, uint hcycle)
{
    if (!w || !m_hcycle_last || (hcycle < m_hring_start) || (hcycle >= m_hcycle_last))
        return 3;
    if (w->n) // n is non-zero: it is a net
        return w->d[hcycle % MAX_WATCH_HISTORY];
    return 4; // The watch is a bus error
}

/*
 * Returns watch data at the specified cycle position. The watch structure can represent a net
 * or a bus. For nets, we simply return the net_t bit stored for that net.
 * For a bus, we need to aggregate all nets that comprise this bus.
 *
 * This function variation returns the aggregate value of a bus; ok is set to the bus width if
 * all of the bus nets are valid, or to zero if the bus value could not be read.
 *
 * This function variation returns a bus.
 */
uint ClassWatch::at(watch *w, uint hcycle, uint &ok)
{
    ok = 0;
    if (!w || !m_hcycle_last || (hcycle < m_hring_start) || (hcycle >= m_hcycle_last))
        return 0;
    if (w->n) // n is non-zero: it is a net
        return 0;
    uint value = 0; // The watch is a bus
    const QVector<net_t> &nets = ::controller.getNetlist().getBus(w->name);
    uint width = nets.count(); // Buses are defined from LSB to MSB; we are filling in bits from MSB
    for (auto n : nets)
    {
        value >>= 1;
        watch *wb = find(n);
        if (wb == nullptr)
            return 0;
        pin_t pin = wb->d[hcycle % MAX_WATCH_HISTORY];
        if (pin > 1)
            return 0;
        value |= uint(pin) << (width - 1);
    }
    ok = width;
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
 * Returns a watch containing a given net number or nullptr
 * XXX make m_watchlist a hash
 */
watch *ClassWatch::find(net_t net)
{
    for (auto &item : m_watchlist)
        if (item.n == net)
            return &item;
    return nullptr;
}

/*
 * Clears all watch data
 */
void ClassWatch::clear()
{
    m_hring_start = 0;
    m_hcycle_last = 0;
    for (auto &watch : m_watchlist)
        watch.clear();
}

/*
 * Returns a list of net and bus names in the watchlist
 */
QStringList ClassWatch::getWatchlist()
{
    QStringList list;
    for (const auto &item : m_watchlist)
        list.append(item.name);
    return list;
}

/*
 * Updates watchlist using a new list of watch names
 * XXX There is a catch: If a bus is added, we need to explicitly add all nets comprising that bus
 *     This will show only the next time the Watch dialog is opened
 */
void ClassWatch::updateWatchlist(QStringList list)
{
    QVector<QString> buses; // List of buses to process later
    QVector<watch> newlist; // New list that we are building
    for (auto name : list)
    {
        watch *w = find(name);
        if (w)
        {
            newlist.append(*w);
            if (w->n == 0) // A bus. queue it to process it later
                buses.append(w->name);
        }
        else
        {
            watch w(name, ::controller.getNetlist().get(name));
            if (w.n == 0) // A bus. queue it to process it later
                buses.append(name);
            newlist.append(w);
        }
    }
    m_watchlist = newlist;

    // Make sure all nets, that belong to buses we added, are also included
    for (auto name : buses)
    {
        QVector<net_t> nets = ::controller.getNetlist().getBus(name);
        for (auto net : nets)
        {
            if (!find(net)) // If this net is not already part of our m_watchlist...
            {
                watch w(::controller.getNetlist().get(net), net);
                m_watchlist.append(w);
            }
        }
    }
}

/*
 * Loads a watchlist from a file
 */
bool ClassWatch::load(QString dir)
{
    QString fileName = dir + "/watchlist.json";
    qInfo() << "Loading watchlist" << fileName;
    QFile loadFile(fileName);
    if (loadFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(data));
        const QJsonObject json = loadDoc.object();

        if (json.contains("watchlist") && json["watchlist"].isArray())
        {
            QJsonArray array = json["watchlist"].toArray();
            m_watchlist.clear();

            for (int i = 0; i < array.size(); i++)
            {
                QString name;
                net_t net = 0;
                QJsonObject obj = array[i].toObject();
                if (obj.contains("net") && obj["net"].isDouble())
                    net = obj["net"].toInt();
                if (obj.contains("name") && obj["name"].isString())
                    name = obj["name"].toString();
                m_watchlist.append( {name, net} );
            }
        }
        return true;
    }
    else
        qWarning() << "Unable to load" << fileName;
    return false;
}

/*
 * Saves the current watchlist to a file
 */
bool ClassWatch::save(QString dir)
{
    QString fileName = dir + "/watchlist.json";
    qInfo() << "Saving watchlist" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
    {
        QJsonObject json;
        QJsonArray jsonArray;
        for (auto w : m_watchlist)
        {
            QJsonObject obj;
            obj["name"] = w.name;
            obj["net"] = w.n;
            jsonArray.append(obj);
        }
        json["watchlist"] = jsonArray;

        QJsonDocument saveDoc(json);
        saveFile.write(saveDoc.toJson());
        return true;
    }
    else
        qWarning() << "Unable to save" << fileName;
    return false;
}
