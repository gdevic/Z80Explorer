#include "ClassSimX.h"

#include <algorithm>
#include <QDebug>
#include <QDir>
#include <QTimer>

#define MAX_TRANSDEFS 9000 // Max number of transistors stored in m_transdefs array
#define MAX_NET 3600 // Max number of nets

ClassSimX::ClassSimX(QObject *parent) : QObject(parent),
    m_transdefs(MAX_TRANSDEFS),
    m_netlist(MAX_NET)
{
}

void ClassSimX::initChip()
{
    Q_ASSERT(ngnd == 1);
    Q_ASSERT(npwr == 2);

    // Initialize GND and Vcc
    m_netlist[ngnd].state = false;
    m_netlist[ngnd].floats = false;
    m_netlist[npwr].state = true;
    m_netlist[npwr].floats = false;

    // Turn off all transistors
    for (auto t : m_transdefs)
        t.on = false;

    // Initialize control pins
    set(0, "reset");
    set(1, "clk");
    set(1, "busrq");
    set(1, "int");
    set(1, "nmi");
    set(1, "wait");
    recalcNetlist(allNets());

    // Propagate the reset before deasserting it
    for (int i=0; i < 8; i++)
        halfCycle();

    set(1, "reset");

    // XXX Test
    // Create a timer that runs the simulation ticks by toggling the clk
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    m_timer->start();
}

void ClassSimX::onTimeout()
{
    halfCycle();

    z80state z80;
    readStatus(z80);
    qInfo() << dumpStatus(z80).split('\n');
}

void ClassSimX::halfCycle()
{
    set(!readBit("clk"), "clk");
}

/*
 * Sets a named input net to pullup or pulldown status
 */
void ClassSimX::set(bool on, QString name)
{
    if (m_netnames.contains(name))
    {
        net_t n = m_netnames.value(name);
        m_netlist[n].pullup = on;
        m_netlist[n].pulldown = !on;
        QVector<net_t> list {n}; // XXX optimize to send only 1 net
        recalcNetlist(list);
    }
    else
        qWarning() << "set: nonexistent net" << name;
}

bool ClassSimX::getNetValue()
{
    // 1. deal with power connections first
    if (group.contains(ngnd)) return false;
    if (group.contains(npwr)) return true;
    // 2. deal with pullup/pulldowns next
    for (auto i : group)
    {
        auto net = m_netlist[i];
        if (net.pullup) return true;
        if (net.pulldown) return false;
    }
    // 3. resolve connected set of floating nodes
    // based on state of largest (by #connections) node
    // (previously this was any node with state true wins)
    auto max_state = false;
    auto max_conn = 0;
    for (auto i : group)
    {
        auto net = m_netlist[i];
        auto conn = net.gates.count() + net.c1c2s.count();
        if (conn > max_conn)
        {
            max_conn = conn;
            max_state = net.state;
        }
    }
    return max_state;
}

void ClassSimX::recalcNetlist(QVector<net_t> list)
{
    recalcList.clear();
    for (int i=0; i<100 && list.count(); i++) // loop limiter
    {
        for (auto n : list)
            recalcNet(n);
        list = recalcList;
        recalcList.clear();
    }
}

void ClassSimX::recalcNet(net_t n)
{
    if ((n==ngnd) || (n==npwr)) return;
    getNetGroup(n);
    auto newState = getNetValue();
    for (auto i : group)
    {
        auto &net = m_netlist[i];
        if (net.state == newState) continue;
        net.state = newState;
        for (trans *t : net.gates)
        {
            if (net.state)
                setTransOn(*t);
            else
                setTransOff(*t);
        }
    }
}

QVector<net_t> ClassSimX::allNets()
{
    QVector<net_t> nets;
    for (net_t n=0; n < m_netlist.count(); n++)
    {
        if ((n==ngnd) || (n==npwr) || (m_netlist[n].gates.count()==0 && m_netlist[n].c1c2s.count()==0))
            continue;
        nets.append(n);
    }
    return nets;
}

void ClassSimX::setTransOn(class trans &t)
{
    if (t.on) return;
    t.on = true;
    addRecalcNet(t.c1);
}

void ClassSimX::setTransOff(class trans &t)
{
    if (!t.on) return;
    t.on = false;
    addRecalcNet(t.c1);
    addRecalcNet(t.c2);
}

void ClassSimX::addRecalcNet(net_t n)
{
    if ((n==ngnd) || (n==npwr)) return;
    if (!recalcList.contains(n))
        recalcList.append(n);
}

void ClassSimX::getNetGroup(net_t n)
{
    group.clear();
    addNetToGroup(n);
}

void ClassSimX::addNetToGroup(net_t n)
{
    if (group.contains(n)) return;
    group.append(n);
    if ((n==ngnd) || (n==npwr)) return;
    for (trans *t : m_netlist[n].c1c2s)
    {
        if (!t->on) continue;
        net_t other = 0;
        if (t->c1 == n) other = t->c2;
        if (t->c2 == n) other = t->c1;
        if (other)
            addNetToGroup(other);
    }
}

/*
 * Attempts to load all required resources
 */
bool ClassSimX::loadResources(QString dir)
{
    qInfo() << "Loading simX resources from " << dir;
    if (loadNodenames(dir) && ngnd && npwr && loadTransdefs(dir) && loadPullups(dir))
    {
        qInfo() << "Completed loading simX resources";

        initChip();

        return true;
    }
    else
        qWarning() << "Loading simX resource failed";
    return false;
}

bool ClassSimX::loadNodenames(QString dir)
{
    QString nodenames_file = dir + "/nodenames.js";
    qInfo() << "Loading pads from" << nodenames_file;
    QFile file(nodenames_file);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        m_netnames.clear();
        while(!in.atEnd())
        {
            line = in.readLine();
            if (!line.startsWith('/') && line.indexOf(':') != -1)
            {
                line.chop(1);
                list = line.split(':');
                if (list.length()==2)
                {
                    QString key = QString(list[0]);
                    if (key.startsWith('_')) // We remove _ prefix from the node names
                        key.remove(0,1);
                    if (!m_netnames.contains(key))
                        m_netnames[key] = list[1].toUInt();
                    else
                        qWarning() << "Duplicate key " << key;
                }
                else
                    qWarning() << "Invalid line " << list;
            }
            else
                qDebug() << "Skipping " << line;
        }
        file.close();
        qInfo() << "Loaded pads";
        ngnd = m_netnames["vss"];
        npwr = m_netnames["vcc"];
        return true;
    }
    else
        qWarning() << "Error opening nodenames.js";
    return false;
}

/*
 * Loads transdefs.js
 * Creates m_transdefs with transistor connections
 * Creates m_netlist with connections to transistors
 */
bool ClassSimX::loadTransdefs(QString dir)
{
    QString transdefs_file = dir + "/transdefs.js";
    qInfo() << "Loading " << transdefs_file;
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
                    Q_ASSERT(i < MAX_TRANSDEFS);
                    trans *p = &m_transdefs[i];

                    p->name = list[0]; // XXX Temp to aid debugging
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
                    qWarning() << "Invalid line " << list;
            }
            else
                qDebug() << "Skipping " << line;
        }
        file.close();
        qInfo() << "Loaded " << m_transdefs.count() << " transistor definitions";
        qInfo() << "Max net index" << max;
        net_t count = 0;
        for (auto net : m_netlist)
            count += !!(net.gates.count() || net.c1c2s.count());
        qInfo() << "Number of nets" << count;
        return true;
    }
    else
        qWarning() << "Error opening transdefs.js";
    return false;
}

bool ClassSimX::loadPullups(QString dir)
{
    QString segdefs_file = dir + "/segdefs.js";
    qInfo() << "Loading " << segdefs_file;
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
                    Q_ASSERT(i < MAX_NET);
                    m_netlist[i].pullup = list[1].contains('+');
                }
                else
                    qWarning() << "Invalid line" << line;
            }
        }
        file.close();
        return true;
    }
    else
        qWarning() << "Error opening segdefs.js";
    return false;
}

/*
 * Returns the value on the address bus
 */
uint16_t ClassSimX::readAB()
{
    uint16_t value= 0;
    for (int i=15; i >= 0; --i)
    {
        QString bit_name = "ab" + QString::number(i);
        value <<= 1;
        value |= readBit(bit_name);
    }
    return value;
}

/*
 * Returns a byte value read from the netlist for a particular net bus
 * The bus needs to be named with the last character selecting the bit, ex. ab0, ab1,...
 */
uint ClassSimX::readByte(QString name)
{
    uint value = 0;
    for (int i=7; i >= 0; --i)
    {
        QString bit_name = name + QString::number(i);
        value <<= 1;
        value |= readBit(bit_name);
    }
    return value;
}

/*
 * Returns a bit value read from the netlist for a particular net
 */
inline uint ClassSimX::readBit(QString name)
{
    if (m_netnames.contains(name))
    {
        net_t n = m_netnames[name];
        Q_ASSERT(n < MAX_NET);
        return !!m_netlist[n].state;
    }
    qWarning() << "readBit: Invalid name" << name;
    return 0xbadbad;
}

/*
 * Returns the pin value
 */
pin_t ClassSimX::readPin(QString name)
{
    if (m_netnames.contains(name))
    {
        net_t n = m_netnames[name];
        Q_ASSERT(n < MAX_NET);
//        if (m_netlist[n].floats) // XXX handle floating node
//            return 2;
        return !!m_netlist[n].state;
    }
    qWarning() << "readPin: Invalid name" << name;
    return -1;
}

/*
 * Reads chip state into a state structure
 */
void ClassSimX::readStatus(z80state &z)
{
    z.af = (readByte("reg_a") << 8) | readByte("reg_f");
    z.bc = (readByte("reg_b") << 8) | readByte("reg_c");
    z.de = (readByte("reg_d") << 8) | readByte("reg_e");
    z.hl = (readByte("reg_h") << 8) | readByte("reg_l");
    z.af2 = (readByte("reg_aa") << 8) | readByte("reg_ff");
    z.bc2 = (readByte("reg_bb") << 8) | readByte("reg_cc");
    z.de2 = (readByte("reg_dd") << 8) | readByte("reg_ee");
    z.hl2 = (readByte("reg_hh") << 8) | readByte("reg_ll");
    z.ix = (readByte("reg_ixh") << 8) | readByte("reg_ixl");
    z.iy = (readByte("reg_iyh") << 8) | readByte("reg_iyl");
    z.sp = (readByte("reg_sph") << 8) | readByte("reg_spl");
    z.ir = (readByte("reg_i") << 8) | readByte("reg_r");
    z.wz = (readByte("reg_w") << 8) | readByte("reg_z");
    z.pc = (readByte("reg_pch") << 8) | readByte("reg_pcl");
    z.ab = readAB();
    z.db = readByte("db");
    for (int i=0; i < 8; i++)
        z._db[i] = readPin("db" + QString::number(i));
    z.clk = readPin("clk");
    z.intr = readPin("int");
    z.nmi = readPin("nmi");
    z.halt = readPin("halt");
    z.mreq = readPin("mreq");
    z.iorq = readPin("iorq");
    z.rd = readPin("rd");
    z.wr = readPin("wr");
    z.busak = readPin("busak");
    z.wait= readPin("wait");
    z.busrq = readPin("busrq");
    z.reset = readPin("reset");
    z.m1 = readPin("m1");
    z.rfsh = readPin("rfsh");
    for (int i=1; i<=6; i++)
    {
        z.m[i-1] = readPin("m" + QString::number(i));
        z.t[i-1] = readPin("t" + QString::number(i));
    }
}

inline QString ClassSimX::hex(uint n, uint width)
{
    QString x = QString::number(n,16).toUpper();
    return QString("%1").arg(x,width,QChar('0'));
}

inline QString ClassSimX::pin(pin_t p)
{
    return p==0 ? "0" : (p==1 ? "1" : (p==2 ? "-" : "?"));
}

/*
 * Returns chip state as a string
 */
QString ClassSimX::dumpStatus(z80state z)
{

    QString s = QString("AF:%1 BC:%2 DE:%3 HL:%4 AF':%5 BC':%6 DE':%7 HL':%8\n").arg
            (hex(z.af,4),hex(z.bc,4),hex(z.de,4),hex(z.hl,4),
             hex(z.af2,4),hex(z.bc2,4),hex(z.de2,4),hex(z.hl2,4));
    s += QString("IX:%1 IY:%2 SP:%3 IR:%4 WZ:%5 PC:%6\n").arg
            (hex(z.ix,4),hex(z.iy,4),hex(z.sp,4),hex(z.ir,4),hex(z.wz,4),hex(z.pc,4));
    s += QString("AB:%1 DB:%2 ").arg(hex(z.ab,4),hex(z.db,4));
    for (auto c : z._db)
        s += pin(c);
    s += QString("\nclk:%1 int:%2 nmi:%3 halt:%4 mreq:%5 iorq:%6 rd:%7 wr:%8 ").arg
            (pin(z.clk),pin(z.intr),pin(z.nmi),pin(z.halt),pin(z.mreq),pin(z.iorq),pin(z.rd),pin(z.wr));
    s += QString("busak:%1 wait:%2 busrq:%3 reset:%4 m1:%5 rfsh:%6\n").arg
            (pin(z.busak),pin(z.wait),pin(z.busrq),pin(z.reset),pin(z.m1),pin(z.rfsh));
#define MT(x,c) (x==0 ? "_" : ((x==1) ? c : "?"))
    s += QString("M:%1%2%3%4%5%6 ").arg(MT(z.m[0],"1"),MT(z.m[1],"2"),MT(z.m[2],"3"),MT(z.m[3],"4"),MT(z.m[4],"5"),MT(z.m[5],"6"));
    s += QString("T:%1%2%3%4%5%6").arg(MT(z.m[0],"1"),MT(z.m[1],"2"),MT(z.m[2],"3"),MT(z.m[3],"4"),MT(z.m[4],"5"),MT(z.m[5],"6"));
#undef MT
    return s;
}
