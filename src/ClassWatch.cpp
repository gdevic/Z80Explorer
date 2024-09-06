#include "ClassController.h"
#include "ClassWatch.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ClassWatch::ClassWatch()
{
    clear();
}

void ClassWatch::onShutdown()
{
    save(m_jsonFile);
}

/*
 * Handles events related to managing the net names
 */
void ClassWatch::onNetName(Netop op, const QString name, const net_t net)
{
    if (op == Netop::SetName) // All newly named nets are automatically added to the watchlist
    {
        Q_ASSERT(find(name) == nullptr);
        m_watchlist.append(watch(name, net));
    }
    else if (op == Netop::Rename)
    {
        watch *w = find(net);
        if (w != nullptr)
            w->name = name;
    }
    else if (op == Netop::DeleteName)
    {
        int i;
        for (i = 0; (i < m_watchlist.count()) && (m_watchlist.at(i).n != net); i++);
        if (i < m_watchlist.count())
            m_watchlist.removeAt(i);
    }
}

/*
 * Adds net watch data to the watch item's buffer at a specified cycle time
 */
void ClassWatch::append(watch *w, uint hcycle, pin_t value)
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
 * or a bus. For nets, we simply return the pin_t bit stored for that net.
 * For a bus, we need to aggregate all nets that comprise this bus.
 *
 * This function variation returns a net.
 */
pin_t ClassWatch::at(watch *w, uint hcycle)
{
    if (!w || !m_hcycle_last || (hcycle < m_hring_start) || (hcycle >= m_hcycle_last))
        return 3; // Invalid
    if (w->n) // n is non-zero: it is a net
        return w->d[hcycle % MAX_WATCH_HISTORY];
    return 4; // The watch is a bus error
}

/*
 * Returns watch data at the specified cycle position. The watch structure can represent a net
 * or a bus. For nets, we simply return the pin_t bit stored for that net.
 * For a bus, we need to aggregate all nets that comprise this bus.
 *
 * This function variation returns the aggregate value of a bus; ok is set to the bus width if
 * all of the bus nets are valid, or to zero if the bus value could not be read.
 *
 * This function variation returns a bus. If any of the nets comprising a bus are hi-Z, then the
 * complete bus is considered hi-Z and the return value will be UINT_MAX.
 */
uint ClassWatch::at(watch *w, uint hcycle, uint &ok)
{
    ok = 0;
    if (!w || !m_hcycle_last || (hcycle < m_hring_start) || (hcycle >= m_hcycle_last))
        return 0; // Invalid bus, or it is a net
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
        if (pin == 2) // Any contributing net that is at hi-Z makes the complete bus being hi-Z
        {
            value = UINT_MAX;
            break;
        }
        else if (pin > 2) // Invalid value
            return 0;
        value |= uint(pin) << (width - 1);
    }
    ok = width;
    return value;
}

/*
 * Returns a watch of a given name or nullptr
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
    for (auto &item : m_watchlist)
        list.append(item.name);
    return list;
}

/*
 * Updates watchlist using a new list of watch names
 * Each new name is also checked against the valid nets/buses and only added if valid
 */
void ClassWatch::updateWatchlist(QStringList list)
{
    ClassNetlist &Net = ::controller.getNetlist();
    QVector<QString> buses; // List of buses to process later
    QVector<watch> newlist; // New list that we are building
    list.removeDuplicates();
    for (auto &name : list)
    {
        net_t net = Net.get(name);
        const QVector<net_t> &nets = Net.getBus(name);
        watch *w = find(name);

        if (net || nets.count()) // The name represents a (valid) net or a bus
        {
            if (w) // Already exists in the watchlist
                newlist.append(*w);
            else // It does not exist, create it
            {
                watch w(name, net);
                newlist.append(w);
            }
            if (!net) // Check and add all nets that comprise this bus
                buses.append(name);
        }
    }
    m_watchlist = newlist;

    // Make sure that all nets, that belong to buses we added, are also included
    for (auto name : buses)
    {
        const QVector<net_t> &nets = Net.getBus(name);
        for (auto net : nets)
        {
            if (!find(net)) // If this net is not already part of our m_watchlist...
            {
                watch w(Net.get(net), net);
                m_watchlist.append(w);
            }
        }
    }
}

/*
 * Loads a watchlist from a file
 */
bool ClassWatch::load(QString fileName)
{
    if (m_jsonFile.isEmpty()) // Set the initial file name
        m_jsonFile = fileName;
    qInfo() << "Loading watchlist from" << fileName;
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
            m_jsonFile = fileName;
            return true;
        }
        else
            qWarning() << "Invalid json file";
    }
    else
        qWarning() << "Unable to load" << fileName;
    return false;
}

/*
 * Saves the current watchlist to a file
 */
bool ClassWatch::save(QString fileName)
{
    qInfo() << "Saving watchlist to" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
    {
        QJsonObject json;
        QJsonArray jsonArray;
        for (auto &w : m_watchlist)
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
