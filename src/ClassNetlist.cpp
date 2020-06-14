#include "ClassNetlist.h"
#include "ClassController.h"
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

        // Check for net names / net numbers consistency
        int strings = 0;
        for (int i=0; i<MAX_NETS; i++)
            if (!m_netnames[i].isEmpty())
                strings++;

        ngnd = get("vss");
        npwr = get("vcc");
        nclk = get("clk");

        qInfo() << "Checking that vss,vcc,clk nets are numbered 1,2,3";
        if (strings == m_netnums.count())
        {
            if (ngnd == 1)
            {
                if (npwr == 2)
                {
                    if (nclk == 3)
                    {
                        if (loadTransdefs(dir) && loadPullups(dir))
                        {
                            qInfo() << "Completed loading netlist resources";
                            return true;
                        }
                        qCritical() << "Loading transistor resource failed";
                    }
                    else
                        qCritical() << "clk expected to be net number 3 but it is" << nclk;
                }
                else
                    qCritical() << "vcc expected to be net number 2 but it is" << npwr;
            }
            else
                qCritical() << "vss expected to be net number 1 but it is" << ngnd;
        }
        else
            qCritical() << "netnames inconsistency:" << strings << "names but" << m_netnums.count() << "nets";
    }
    else
        qCritical() << "Loading netlist resource failed";
    return false;
}

/*
 * Saves custom net names (all new names and overrides of the names defined in nodenames.js file)
 */
bool ClassNetlist::saveNetNames(const QString fileName)
{
    qInfo() << "Saving net names to" << fileName;
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
        for (auto &n : names)
        {
            QString tip = ::controller.getChip().tips.get(m_netnums[n]);
            if (tip.isEmpty())
                out << n << ": " << m_netnums[n] << ",\n";
            else
                out << n << ": " << m_netnums[n] << ", // " << tip << "\n";
        }

        out << "// Buses:\n"; // Write out the buses, sorted alphabetically
        QStringList buses = m_buses.keys();
        buses.sort();
        for (const auto &i : buses)
        {
            QString line = QString("%1: [").arg(i);
            for (auto &net : m_buses[i])
                line.append(QString::number(net) % ",");
            line.chop(1); // Remove that last comma
            line.append("],\n");
            out << line;
        }
        out << "}\n";
        return true;
    }
    return false;
}

/*
 * Load Java-style net names files:
 * 1. nodenames.js : generated by Z80Simulator and it has some duplicate node names aliased to the same net number
 *                   in which case we keep the first name found.
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
            int comment = line.indexOf('/'); // Strip comments
            if (comment != -1)
                line = line.left(comment).trimmed();
            if (line.indexOf(':') != -1)
            {
                line.chop(1); // Remove comma at the end of each line
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
                            // Custom file overrides previously assigned names
                            if (m_netnums.contains(name)) // Deletes the name if it's already in use
                                eventNetName(Netop::DeleteName, QString(), m_netnums[name]);
                            eventNetName(Netop::SetName, name, n);
                        }
                    }
                    else // Load base nodename.js
                    {
                        if (m_netnums.contains(name)) // New name should not already be in use
                            qWarning() << "Duplicate name" << name << "for net" << n << ", already assigned to net" << m_netnums[name];
                        else if (!m_netnames[n].isEmpty()) // The net we are naming should not already have a name
                            qWarning() << "Naming" << name << "but net" << n << "was already assigned a name" << m_netnames[n];
                        else
                        {
                            m_netnames[n] = name;
                            m_netnums[name] = n;
                        }
                    }
                }
                else
                    qWarning() << "Invalid line" << list;
            }
        }
        return true;
    }
    qCritical() << "Error opening" << fileName;
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
        m_transdefs.fill(Trans{}); // Clear the array with the defaults
        uint count = 0;
        m_netlist.fill(Net{});

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
                    tran_t i = tnum.toUInt();
                    Q_ASSERT(i < MAX_TRANS);
                    Trans *p = &m_transdefs[i];

                    p->id = i;
                    p->gate = list[1].toUInt();
                    p->c1 = list[2].toUInt();
                    p->c2 = list[3].toUInt();

                    // Pull-up and pull-down transistors should always have their *second* connection to the power/ground
                    if ((p->c1 == npwr) || (p->c1 == ngnd))
                        std::swap(p->c1, p->c2);
                    max = std::max(max, std::max(p->c1, p->c2)); // Find the max net number

                    // ----- Add the transistor to the netlist -----
                    m_netlist[p->gate].gates.append(p);
                    m_netlist[p->c1].c1c2s.append(p);
                    m_netlist[p->c2].c1c2s.append(p);
                    count++;
                }
                else
                    qWarning() << "Invalid line" << list;
            }
            else
                qDebug() << "Skipping" << line;
        }
        qInfo() << "Loaded" << count << "transistor definitions";
        qInfo() << "Max net index" << max;
        count = std::count_if(m_netlist.begin(), m_netlist.end(), [](Net &net)
            { return !!(net.gates.count() || net.c1c2s.count()); });
        qInfo() << "Number of nets" << count;
        count = std::count_if(m_transdefs.begin(), m_transdefs.end(), [](Trans &t) { return t.id; });
        qInfo() << "Number of transistors" << count;
        return true;
    }
    qCritical() << "Error opening transdefs.js";
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
                    // Net has a (permanent) pull-up resistor and it is high on a power-up
                    m_netlist[i].hasPullup = list[1].contains('+');
                    m_netlist[i].isHigh = list[1].contains('+');
                }
                else
                    qWarning() << "Invalid line" << line;
            }
        }
        uint count = std::count_if(m_netlist.begin(), m_netlist.end(), [](Net &net) { return net.hasPullup; });
        qInfo() << "Number of pullups" << count;
        return true;
    }
    qCritical() << "Error opening segdefs.js";
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
        qDebug() << "Setting net name" << name << "for net" << net;
        Q_ASSERT(!m_netnums.contains(name)); // New name should not be already in use
        Q_ASSERT(m_netnames[net].isEmpty()); // The net we are naming should not already have a name
        m_netnames[net] = name;
        m_netnums[name] = net;
        m_netoverrides[net] = true;
    }
    else if (op == Netop::Rename)
    {
        qDebug() << "Renaming net" << net << "to" << name;
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
        qDebug() << "Deleting name for net" << net;
        Q_ASSERT(!m_netnames[net].isEmpty()); // The net which name we are deleting should already have a name
        QString oldName = m_netnames[net];
        m_netnums.remove(oldName);
        m_netnames[net] = QString();
        m_netoverrides[net] = false;
    }
}

/*
 * Returns sorted net names for each net on the list
 */
const QStringList ClassNetlist::get(const QVector<net_t> &nets)
{
    QStringList list;
    for (net_t n : nets)
    {
        QString name = get(n);
        list.append(name.isEmpty() ? QString::number(n) : name);
    }

    QCollator collator; // Sort in the correct numerical order, naturally (so, after "a9" comes "a10")
    collator.setNumericMode(true);
    std::sort(list.begin(), list.end(), collator);

    return list;
}

/*
 * Returns a sorted list of unique nets that the given net is driving
 */
const QVector<net_t> ClassNetlist::netsDriving(net_t n)
{
    QVector<net_t> nets;
    const QVector<Trans *> &gates = m_netlist[n].gates;

    for (const auto t : gates)
    {
        if ((t->c1 > 2) && !nets.contains(t->c1)) // c1 is the source
            nets.append(t->c1);
        if ((t->c2 > 2) && !nets.contains(t->c2)) // c2 is the drain
            nets.append(t->c2);
    }
    std::sort(nets.begin(), nets.end()); // Sorting numbers only
    return nets;
}

/*
 * Returns a sorted list of unique nets that the given net is being driven by
 */
const QVector<net_t> ClassNetlist::netsDriven(net_t n)
{
    QVector<net_t> nets;
    for (auto &t : m_netlist[n].c1c2s)
        if (!nets.contains(t->gate))
            nets.append(t->gate);
    std::sort(nets.begin(), nets.end()); // Sorting numbers only
    return nets;
}

/*
 * Returns the net extended logic state (including hi-Z)
 */
inline pin_t ClassNetlist::getNetStateEx(net_t n)
{
    // Every transistor in the contributing nets needs to be off for this net to be hi-Z
    for (auto &tran : m_netlist[n].c1c2s)
        if (tran->on) return !!m_netlist[n].state;
    return 2;
}

/*
 * Adds bus by name and a set of nets listed by their name
 */
void ClassNetlist::addBus(const QString &name, const QStringList &list)
{
    // Replace net names with net numbers
    QVector<net_t> nets;
    for (const auto &name : list)
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
        value |= !!readBit(QString("ab" % QString::number(i)));
    }
    return value;
}

/*
 * Returns a byte value read from the netlist for a particular net bus
 * The bus needs to be named with the last character selecting the bit, ex. ab0, ab1,...
 */
uint8_t ClassNetlist::readByte(const QString &name)
{
    uint value = 0;
    for (int i=7; i >= 0; --i)
    {
        value <<= 1;
        value |= !!readBit(QString(name % QString::number(i)));
    }
    return value;
}

/*
 * Returns a bit value read from the netlist for a particular net, by net name
 */
pin_t ClassNetlist::readBit(const QString &name)
{
    net_t n = get(name);
    Q_ASSERT(n < MAX_NETS);
    if (Q_UNLIKELY(m_netlist[n].floats))
        return getNetStateEx(n);
    return m_netlist[n].state;
}

/*
 * Returns a bit value read from the netlist for a particular net, by net number
 */
pin_t ClassNetlist::readBit(const net_t n)
{
    Q_ASSERT(n < MAX_NETS);
    if (Q_UNLIKELY(m_netlist[n].floats))
        return getNetStateEx(n);
    return m_netlist[n].state;
}

/*
 * Returns basic net information as string
 */
const QString ClassNetlist::netInfo(net_t net)
{
    if (net < MAX_NETS)
    {
        // Transistor numbers for which this net is either a source or a drain
        QStringList c1c2s;
        for (auto &t : m_netlist[net].c1c2s)
            c1c2s.append( QString::number(t - &m_transdefs[0]) );
        // Transistor numbers for which this net is a gate
        QStringList gates;
        for (auto &t : m_netlist[net].gates)
            gates.append( QString::number(t - &m_transdefs[0]) );

        QString s = ::controller.getNetlist().get(net);
        if (!s.isEmpty())
            s = s % ":";
        s = s % QString("%1: pulled-up:%2").arg(net).arg(m_netlist[net].hasPullup)
        % QString("\nstate:%1 can-float:%2 is-high:%3 is-low:%4")
                .arg(m_netlist[net].state).arg(m_netlist[net].floats).arg(m_netlist[net].isHigh).arg(m_netlist[net].isLow)
        % "\nsource/drain:"
        % c1c2s.join(",")
        % "\nto-gates:"
        % gates.join(",");
        return s;
    }
    return QString("Invalid net number");
}

/*
 * Returns basic transistor information as string
 */
const QString ClassNetlist::transInfo(tran_t t)
{
    if ((t < MAX_TRANS) && m_transdefs[t].id)
        return QString("gate:%2 c1:%3 c2:%4 ON:%5").arg(m_transdefs[t].gate).arg(m_transdefs[t].c1).arg(m_transdefs[t].c2).arg(m_transdefs[t].on);
    return QString("Invalid transistor number");
}

/*
 * Generates a logic equation for a net
 * Parses the netlist, starting at the given node, and creates a logic equation
 * that includes all the nets driving it. The tree stops with the nets that are
 * chosen as meaningful end points: clk, t1..t6, m1..m6 and the PLA signals.
 */
QVector<net_t> visited; // Avoid loops by keeping nets that are already visited

Logic *ClassNetlist::getLogicTree(net_t net)
{
    visited.clear();
    Logic *root = new Logic(net);
    parse(root);
    // Fixup for the root node which might have been reused for a NOR gate
    if (root->op != LogicOp::Nop)
    {
        Logic *root2 = new Logic(net);
        root2->children.append(root);
        return root2;
    }
    return root;
}

QString ClassNetlist::equation(net_t net)
{
    Logic *lr = getLogicTree(net);
    QString equation = lr->name % " = " % Logic::flatten(lr);
    Logic::purge(lr);
    qDebug() << equation;
    return equation;
}

/*
 * Optimizes, in place, logic tree by coalescing suitable nodes
 */
void ClassNetlist::optimizeLogicTree(Logic *lr)
{
    if ((lr->children.count() == 1) && (lr->op == LogicOp::Inverter))
    {
        Logic *next = lr->children[0];
        LogicOp nextOp;
        switch (next->op)
        {
            case LogicOp::And: nextOp = LogicOp::Nand; break;
            case LogicOp::Nand: nextOp = LogicOp::And; break;
            case LogicOp::Or: nextOp = LogicOp::Nor; break;
            case LogicOp::Nor: nextOp = LogicOp::Or; break;
            default: nextOp = LogicOp::Nop; break;
        }
        if (nextOp != LogicOp::Nop)
        {
            lr->op = nextOp;
            lr->tag = next->tag;
            lr->leaf = next->leaf;
            lr->children = next->children;
            delete next;
        }
    }
    for (auto &k : lr->children)
        optimizeLogicTree(k);
}

void ClassNetlist::parse(Logic *root)
{
    if (root->leaf)
        return;

    net_t net0id = root->net;
    Net &net0 = m_netlist[net0id];

    // Recognize some common patterns:
    //-------------------------------------------------------------------------------
    // Detect clock gating
    //-------------------------------------------------------------------------------
    if ((net0.c1c2s.count() == 1) && (net0.c1c2s[0]->gate == nclk))
    {
        net_t netid = (net0.c1c2s[0]->c1 == net0id) ? net0.c1c2s[0]->c2 : net0.c1c2s[0]->c1; // Pick the "other" net
        qDebug() << net0id << "Clock gate to" << netid;
        Logic *node = new Logic(netid);
        root->children.append(new Logic(nclk)); // Add clk subnet
        root->children.append(node);
        root->op = LogicOp::And;
        parse(node);
        return;
    }

    //-------------------------------------------------------------------------------
    // Detect a lone Inverter driver
    //-------------------------------------------------------------------------------
    if ((net0.c1c2s.count() == 1) && (net0.c1c2s[0]->c2 == ngnd))
    {
        net_t netgt = net0.c1c2s[0]->gate;
        qDebug() << net0id << "Inverter to" << netgt;
        Logic *node = new Logic(netgt);
        root->children.append(node);
        root->op = LogicOp::Inverter;
        parse(node);
        return;
    }

    //-------------------------------------------------------------------------------
    // Detect Pulled-up Inverter driver (ex. 1304)
    //-------------------------------------------------------------------------------
    if ((net0.gates.count() == 0) && (net0.c1c2s.count() == 2) && net0.hasPullup)
    {
        // No gates but two source/drain connections: we need to find out which transistor we need to follow
        net_t netid = 0;
        bool x11 = visited.contains(net0.c1c2s[0]->c1);
        bool x12 = visited.contains(net0.c1c2s[0]->c2);
        bool x21 = visited.contains(net0.c1c2s[1]->c1);
        bool x22 = visited.contains(net0.c1c2s[1]->c2);
        bool gnd1 = net0.c1c2s[0]->c2 == ngnd;
        bool gnd2 = net0.c1c2s[1]->c2 == ngnd;
        // Identify which way we go
        if (gnd1 && x21 && x22)
            netid = net0.c1c2s[0]->gate;
        if (gnd2 && x11 && x12)
            netid = net0.c1c2s[1]->gate;
        Q_ASSERT(netid);
        qDebug() << net0id << "Pulled-up Inverted to" << netid;
        Logic *node = new Logic(netid);
        root->children.append(node);
        root->op = LogicOp::Inverter;
        parse(node);
        return;
    }

    //-------------------------------------------------------------------------------
    // Detect Push-Pull driver
    //-------------------------------------------------------------------------------
    if (net0.c1c2s.count() == 2)
    {
        if ((net0.c1c2s[0]->c2 <= npwr) && (net0.c1c2s[1]->c2 <= npwr))
        {
            net_t netid = 0;
            net_t net01id = net0.c1c2s[0]->gate;
            net_t net02id = net0.c1c2s[1]->gate;
            Net &net01 = m_netlist[net01id];
            Net &net02 = m_netlist[net02id];
            // Identify the "short" path
            if ((net01.gates.count() == 1) && (net01.c1c2s.count() == 1) && (net01.c1c2s[0]->c2 == ngnd) && (net01.c1c2s[0]->gate == net02id))
                netid = net02id;
            if ((net02.gates.count() == 1) && (net02.c1c2s.count() == 1) && (net02.c1c2s[0]->c2 == ngnd) && (net02.c1c2s[0]->gate == net01id))
                netid = net01id;
            if (netid)
            {
                qDebug() << "Push-Pull driver" << net01id << net02id << "source net" << netid;
                Logic *node = new Logic(netid);
                root->children.append(node);
                root->op = LogicOp::Inverter;
                parse(node);
                return;
            }
        }
    }

    //-------------------------------------------------------------------------------
    // *All* source/drain connections could simply form a NOR gate with 2 or more inputs
    // This is a shortcut to a more generic case handled below (with mixed NOR/NAND/AND gates)
    //-------------------------------------------------------------------------------
    auto i = std::count_if(net0.c1c2s.begin(), net0.c1c2s.end(), [=](Trans *t) { return t->c2 == ngnd; } );
    if (i == net0.c1c2s.count())
    {
        qDebug() << "NOR gate" << net0id << "with fan-in of" << i;
        for (auto k : net0.c1c2s)
        {
            Logic *node = new Logic(k->gate);
            root->children.append(node);
        }
        root->op = LogicOp::Nor;
        return;
    }

    // Loop over all transistors for which the current net is the source/drain
    for (auto t1 : net0.c1c2s)
    {       
        net_t net1gt = t1->gate;
        net_t net1id = (t1->c1 == net0id) ? t1->c2 : t1->c1; // Pick the "other" net
        Net &net1 = m_netlist[net1id];

        qDebug() << "Processing net" << net0id << "at transistor" << t1->id;
        //-------------------------------------------------------------------------------
        // Ignore pull-up inputs
        //-------------------------------------------------------------------------------
        if (t1->c2 == npwr)
        {
            qDebug() << "Connection to npwr ignored";
            continue;
        }

        Logic *node = new Logic(net1gt);
        root->children.append(node);

        //-------------------------------------------------------------------------------
        // Single input to a (root) NOR gate
        //-------------------------------------------------------------------------------
        // This could be an inverter but it's not since we already detected a lone input inverter driver
        // So, this is one NOR gate input in a mixed net configuration
        if (t1->c2 == ngnd)
        {
            qDebug() << "NOR gate input" << net1gt;
            parse(node);
            continue;
        }

        //-------------------------------------------------------------------------------
        // Complex NAND gate extends for 2 pass-transistor nets (ex. net 215)
        //-------------------------------------------------------------------------------
        if ((net1.gates.count() == 0) && (net1.c1c2s.count() == 2) && (net1.hasPullup == false))
        {
            Trans *t2 = (net1.c1c2s[0]->id == t1->id) ? net1.c1c2s[1] : net1.c1c2s[0];
            net_t net2gt = t2->gate;
            net_t net2id = (t2->c1 == net1id) ? t2->c2 : t2->c1; // Pick the "other" net
            Net &net2 = m_netlist[net2id];

            if (net2id == ngnd)
            {
                qDebug() << "NAND gate with 2 inputs" << net1gt << net2gt;

                node->op = LogicOp::Nand;
                node->children.append(new Logic(net1gt));
                node->children.append(new Logic(net2gt));

                // Recurse into each of the contributing nets
                parse(node->children[0]);
                parse(node->children[1]);
                continue;
            }
            //-------------------------------------------------------------------------------
            // Complex NAND gate extends for 3 pass-transistor nets (ex. net 235)
            //-------------------------------------------------------------------------------
            if ((net2.gates.count() == 0) && (net2.c1c2s.count() == 2) && (net2.hasPullup == false))
            {
                Trans *t3 = (net2.c1c2s[0]->id == t2->id) ? net2.c1c2s[1] : net2.c1c2s[0];
                net_t net3gt = t3->gate;
                net_t net3id = (t3->c1 == net2id) ? t3->c2 : t3->c1; // Pick the "other" net

                if (net3id == ngnd)
                {
                    qDebug() << "NAND gate with 3 inputs" << net1gt << net2gt << net3gt;

                    node->op = LogicOp::Nand;
                    node->children.append(new Logic(net1gt));
                    node->children.append(new Logic(net2gt));
                    node->children.append(new Logic(net3gt));

                    // Recurse into each of the contributing nets
                    parse(node->children[0]);
                    parse(node->children[1]);
                    parse(node->children[2]);
                    continue;
                }
                // Have not seen a 4 pass-transistor NAND gate in Z80, yet
                Q_ASSERT(0);
            }
            // Any other topological features that start as 2 pass-transistor nets?
            Q_ASSERT(0);
        }

        //-------------------------------------------------------------------------------
        // Transistor is a simple AND gate
        //-------------------------------------------------------------------------------
        qDebug() << "AND gate with 2 input nets" << net1gt << net1id;
        node->op = LogicOp::And;
        node->children.append(new Logic(net1gt));
        node->children.append(new Logic(net1id));

        // Recurse into each of the contributing nets
        parse(node->children[0]);
        parse(node->children[1]);
    }
    // Join parallel transistors into one (root) NOR gate
    if (root->children.count() > 1)
        root->op = LogicOp::Nor;
}

Logic::Logic(net_t n, LogicOp op) : net(n), op(op)
{
    // Terminating nets: ngnd,npwr,clk, t1..t6,m1..m6, int_reset, ... any pla wire (below)
    const static QVector<net_t> term {0,1,2,3, 115,137,144,166,134,168,155,173,163,159,209,210, 95};
    name = ::controller.getNetlist().get(n);
    if (name.isEmpty())
        name = QString::number(n);
    // We stop processing nodes at a leaf node which is either one of the predefined nodes or a detected loop
    leaf = term.contains(n) || name.startsWith("pla");
    if (visited.contains(n))
        leaf = true;
    else
        visited.append(n);
}
