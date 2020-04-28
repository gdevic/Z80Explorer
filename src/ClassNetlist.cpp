#include "ClassNetlist.h"
#include <QCollator>
#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QStringBuilder>

ClassNetlist::ClassNetlist():
    m_transdefs(MAX_TRANS),
    m_netlist(MAX_NETS)
{
}

void ClassNetlist::onShutdown()
{
    QSettings settings;
    QString path = settings.value("ResourceDir").toString();
    Q_ASSERT(!path.isEmpty());
    saveNetNames(path + "/netnames.js");
}

bool ClassNetlist::loadResources(const QString dir)
{
    qInfo() << "Loading netlist resources from" << dir;
    if (loadNetNames(dir + "/nodenames.js", false))
    {
        // Load (optional) custom net names file
        loadNetNames(dir + "/netnames.js", true);

        ngnd = get("vss");
        npwr = get("vcc");

        if (ngnd && npwr && loadTransdefs(dir) && loadPullups(dir))
        {
            qInfo() << "Completed loading netlist resources";
            return true;
        }
        qWarning() << "Loading transistor resource failed";
    }
    else
        qWarning() << "Loading netlist resource failed";
    return false;
}

/*
 * Saves custom net names (all new names and overrides of the names defined in nodenames.js file)
 */
bool ClassNetlist::saveNetNames(const QString fileName)
{
    qInfo() << "Saving net names" << fileName;
    QFile file(fileName);
    if (file.open(QFile::WriteOnly | QFile::Text))
    {
        QTextStream out(&file);
        out << "// This file contains custom net names, overrides of the names defined in nodenames.js\n";
        out << "// and definitions of buses (collections of nets). Modify by hand only when the app is not running.\n";
        out << "var nodenames_override = {\n";

        QStringList names; // Write out custom names, sorted alphabetically
        for (int i=0; i<MAX_NETS; i++)
        {
            if (m_netoverrides[i])
                names.append(m_netnames[i]);
        }
        QCollator collator; // Sort in the correct numerical order, naturally (so, after "a9" comes "a10")
        collator.setNumericMode(true);
        std::sort(names.begin(), names.end(), collator);
        for (auto n : names)
            out << n << ": " << QString::number(m_netnums[n]) << ",\n";

        out << "// Buses:\n"; // Write out the buses, sorted alphabetically
        QStringList buses = m_buses.keys();
        buses.sort();
        for (const auto &i : buses)
        {
            QString line = QString("%1: [").arg(i);
            for (auto net : m_buses[i])
                line.append(QString::number(net) % ",");
            line.chop(1); // Remove that last comma
            line.append("],\n");
            out << line;
        }
        out << "}\n";
        file.close();
        return true;
    }
    return false;
}

/*
 * Load Java-style net names files:
 * 1. nodenames.js : generated by Z80Simulator and it has some duplicate node names aliased to the same net number
 *                   in which case we keep only the last name found.
 * 2. netnames.js : this is our custom set of net names, it overrides nodenames.js
 */
bool ClassNetlist::loadNetNames(const QString fileName, bool loadCustom)
{
    qInfo() << "Loading" << fileName;
    QFile file(fileName);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        while(!in.atEnd())
        {
            line = in.readLine();
            if (!line.startsWith('/') && line.indexOf(':') != -1)
            {
                line.chop(1);
                list = line.split(':', QString::SkipEmptyParts);
                if (list.length()==2)
                {
                    QString name = list[0].trimmed();
                    net_t n = list[1].toUInt();
                    // We are loading 2 different files: nodenames.js and custom netnames.js with updates and overrides
                    // Custom file can also contain bus definitions
                    if (loadCustom)
                    {
                        // Bus is the collections of 2 or more individual nets
                        QStringList buslist = list[1].replace('[',' ').replace(']',' ').split(",", QString::SkipEmptyParts);
                        if (buslist.count() > 1)
                        {
                            QVector<net_t> nets;
                            for (const auto &n : buslist)
                                nets.append(n.toUInt());
                            m_buses[name] = nets;
                        }
                        else
                        {
                            // Custom file overrides previously loaded names
                            m_netnames[n] = name;
                            m_netoverrides[n] = true;
                            m_netnums[name] = n;
                        }
                    }
                    else // Load base nodename.js
                    {
                        // Each net number can have only 1 name, in the case of a duplicate, store the last name
                        if (!m_netnames[n].isEmpty())
                            qWarning() << "Duplicate name" << name << "for net" << n << ", was" << m_netnames[n];
                        m_netnames[n] = name;

                        // Each net number can be mapped to only one name
                        if (m_netnums.contains(name))
                            qWarning() << "Duplicate mapping of net" << n << "to" << name << ", was" << m_netnums[name];
                        else
                            m_netnums[name] = n;
                    }
                }
                else
                    qWarning() << "Invalid line" << list;
            }
            else
                qDebug() << "Skipping" << line;
        }
        file.close();
        return true;
    }
    qWarning() << "Error opening" << fileName;
    return false;
}

/*
 * Loads transdefs.js
 * Creates m_transdefs with transistor connections
 * Creates m_netlist with connections to transistors
 */
bool ClassNetlist::loadTransdefs(const QString dir)
{
    QString transdefs_file = dir + "/transdefs.js";
    qInfo() << "Loading" << transdefs_file;
    QFile file(transdefs_file);
    net_t max = 0;
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        m_transdefs.fill(trans{}); // Clear the array with the defaults
        m_netlist.fill(net{});

        while(!in.atEnd())
        {
            line = in.readLine();
            if (line.startsWith('['))
            {
                line.replace('[', ' ').replace(']', ' '); // Make it a simple list of numbers
                line.chop(2);
                list = line.split(',', QString::SkipEmptyParts);
                if (list.length()==14 && list[0].length() > 2)
                {
                    // ----- Add the transistor to the transistor array -----
                    QString tnum = list[0].mid(3, list[0].length() - 4);
                    uint i = tnum.toUInt();
                    Q_ASSERT(i < MAX_TRANS);
                    trans *p = &m_transdefs[i];

                    p->gate = list[1].toUInt();
                    p->c1 = list[2].toUInt();
                    p->c2 = list[3].toUInt();

                    // Fixups for pull-up and pull-down transistors
                    if ((p->c1 == ngnd) || (p->c1 == npwr))
                        std::swap(p->c1, p->c2);
                    max = std::max(max, std::max(p->c1, p->c2)); // Find the max net number

                    // ----- Add the transistor to the netlist -----
                    m_netlist[p->gate].gates.append(p);
                    m_netlist[p->c1].c1c2s.append(p);
                    m_netlist[p->c2].c1c2s.append(p);
                }
                else
                    qWarning() << "Invalid line" << list;
            }
            else
                qDebug() << "Skipping" << line;
        }
        file.close();
        qInfo() << "Loaded" << m_transdefs.count() << "transistor definitions";
        qInfo() << "Max net index" << max;
        net_t count = 0;
        for (const auto &net : m_netlist)
            count += !!(net.gates.count() || net.c1c2s.count());
        qInfo() << "Number of nets" << count;
        return true;
    }
    qWarning() << "Error opening transdefs.js";
    return false;
}

/*
 * Pullups are defined in the segdefs.js file
 */
bool ClassNetlist::loadPullups(const QString dir)
{
    QString segdefs_file = dir + "/segdefs.js";
    qInfo() << "Loading" << segdefs_file;
    QFile file(segdefs_file);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        while (!in.atEnd())
        {
            line = in.readLine();
            if (line.startsWith('['))
            {
                line = line.mid(2, line.length() - 4);
                list = line.split(',');
                if (list.length() > 4)
                {
                    uint i = list[0].toUInt();
                    Q_ASSERT(i < MAX_NETS);
                    m_netlist[i].pullup = list[1].contains('+');
                }
                else
                    qWarning() << "Invalid line" << line;
            }
        }
        file.close();
        return true;
    }
    qWarning() << "Error opening segdefs.js";
    return false;
}

/*
 * Returns a list of net and bus names concatenated
 */
QStringList ClassNetlist::getNetnames()
{
    QStringList nodes = m_netnums.keys();
    QStringList buses = m_buses.keys();
    return nodes + buses;
}

/*
 * Handles requests to manage net names (called only by the controller class)
 */
void ClassNetlist::eventNetName(Netop op, const QString name, const net_t net)
{
    if (op == Netop::SetName)
    {
        qDebug() << "Setting net name" << name << "for net" << QString::number(net);
        Q_ASSERT(!m_netnums.contains(name)); // New name should not be already in use
        Q_ASSERT(m_netnames[net].isEmpty()); // The net we are naming should not already have a name
        m_netnames[net] = name;
        m_netnums[name] = net;
        m_netoverrides[net] = true;
    }
    else if (op == Netop::Rename)
    {
        qDebug() << "Renaming net" << QString::number(net) << "to" << name;
        Q_ASSERT(!m_netnums.contains(name)); // New name should not be already in use
        Q_ASSERT(!m_netnames[net].isEmpty()); // The net we are naming should have a name
        QString oldName = m_netnames[net];
        m_netnums.remove(oldName);
        m_netnames[net] = name;
        m_netnums[name] = net;
        m_netoverrides[net] = true;
    }
    else if (op == Netop::DeleteName)
    {
        qDebug() << "Deleting name for net" << QString::number(net);
        Q_ASSERT(!m_netnames[net].isEmpty()); // The net which name we are deleting should already have a name
        QString oldName = m_netnames[net];
        m_netnums.remove(oldName);
        m_netnames[net] = QString();
        m_netoverrides[net] = false;
    }
}

/*
 * Returns net names for each net in the list
 */
const QStringList ClassNetlist::get(const QVector<net_t> &nets)
{
    QStringList list;
    for (net_t n : nets)
    {
        QString name = get(n);
        list.append(name.isEmpty() ? QString::number(n) : name);
    }
    return list;
}

/*
 * Returns a sorted list of nets that the given net is driving
 */
const QVector<net_t> ClassNetlist::netsDriving(net_t n)
{
    QVector<net_t> nets;
    const QVector<trans *> &gates = m_netlist[n].gates;

    for (const auto t : gates)
    {
        if (t->c1 > 2) // c1 is the source
            nets.append(t->c1);
        if (t->c2 > 2) // c2 is the drain
            nets.append(t->c2);
    }
    std::sort(nets.begin(), nets.end());
    return nets;
}

/*
 * Returns a sorted list of nets that the given net is being driven by
 */
const QVector<net_t> ClassNetlist::netsDriven(net_t n)
{
    QVector<net_t> nets;
    for (const auto &t : m_netlist[n].c1c2s)
        nets.append(t->gate);
    std::sort(nets.begin(), nets.end());
    return nets;
}

/*
 * Adds bus by name and a set of nets listed by their name
 */
void ClassNetlist::addBus(const QString &name, const QStringList &netslist)
{
    // Replace net names with net numbers
    QVector<net_t> nets;
    for (const auto &name : netslist)
        nets.append(get(name));
    m_buses[name] = nets;
}

/*
 * Returns the value on the address bus
 */
uint16_t ClassNetlist::readAB()
{
    uint16_t value= 0;
    for (int i=15; i >= 0; --i)
    {
        value <<= 1;
        value |= readBit(QString("ab" % QString::number(i)));
    }
    return value;
}

/*
 * Returns a byte value read from the netlist for a particular net bus
 * The bus needs to be named with the last character selecting the bit, ex. ab0, ab1,...
 */
uint ClassNetlist::readByte(const QString &name)
{
    uint value = 0;
    for (int i=7; i >= 0; --i)
    {
        value <<= 1;
        value |= readBit(QString(name % QString::number(i)));
    }
    return value;
}

/*
 * Returns a bit value read from the netlist for a particular net
 */
//uint ClassNetlist::readBit(QString name)
//{
//    net_t n = get(name);
//    if (n)
//    {
//        Q_ASSERT(n < MAX_NET);
//        return !!m_netlist[n].state;
//    }
//    qWarning() << "readBit: Invalid name" << name;
//    return 0xbadbad;
//}

/*
 * Returns the pin value
 */
pin_t ClassNetlist::readPin(const QString &name)
{
    net_t n = get(name);
    if (n)
    {
        Q_ASSERT(n < MAX_NETS);
//        if (m_netlist[n].floats) // XXX handle floating node
//            return 2;
        return !!m_netlist[n].state;
    }
    qWarning() << "readPin: Invalid name" << name;
    return 3;
}
