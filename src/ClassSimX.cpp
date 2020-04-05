#include "ClassSimX.h"
#include "ClassController.h"
#include <algorithm>
#include <QDebug>
#include <QDir>
#include <QFuture>
#include <QStringBuilder>
#include <QTimer>
#include <QtConcurrent>

#define MAX_TRANSDEFS 9000 // Max number of transistors stored in m_transdefs array
#define MAX_NET 3600 // Max number of nets

ClassSimX::ClassSimX():
    m_transdefs(MAX_TRANSDEFS),
    m_netlist(MAX_NET)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &ClassSimX::onTimeout);
}

// XXX Remove this timer here and implement it somewhere else (?)
void ClassSimX::onTimeout()
{
    z80state z80;
    readState(z80);
    QStringList s = z80state::dumpState(z80).split("\n");
    s.removeAt(4); // Remove pins section
    qDebug() << s << "Half-Cycles:" % QString::number(m_hcycletotal) << (m_hcyclecnt / 2.0) / (m_time.elapsed() / 1000.0) << " Hz";
    if (m_runcount <= 0)
        m_timer.stop();
}

void ClassSimX::doRunsim(uint ticks)
{
    if (!m_runcount && !ticks) // For Stop signal (ticks=0), do nothing if the sim thread is not running
        return;
    if (m_runcount) // If the sim thread is already running, simply set the new tick counter
        m_runcount = ticks;
    else
    {
        if (ticks < 3) // Optimize for special cases of up to 2 half-cycle steps, makes the interaction more responsive
        {
            while (ticks--)
                halfCycle();
            emit runStopped();
            onTimeout(); // XXX Can we get rid of this chain?
        }
        else
        {
            m_runcount = ticks; // If the sim thread is not running, start it using the new tick counter
            m_timer.start();
            m_time.start();
            m_hcyclecnt = 0;
            // Code in this block will run in another thread
            QFuture<void> future = QtConcurrent::run([=]()
            {
                while (--m_runcount >= 0)
                    halfCycle();
                m_runcount = 0;
                emit runStopped();
            });
        }
    }
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

    doReset();
}

/*
 * Run chip reset sequence
 */
void ClassSimX::doReset()
{
    // If the chip is running, stop it instead
    if (m_runcount)
    {
        m_runcount = 0; // XXX This is not enough since the runnig thread may continue for a cycle or two
        return;
    }

    // Initialize control pins
    set(0, "_reset");
    set(1, "clk");
    set(1, "busrq");
    set(1, "int");
    set(1, "nmi");
    set(1, "wait");
    recalcNetlist(allNets());

    // Propagate the reset before deasserting it
    for (int i=0; i < 8; i++)
        halfCycle();
    m_hcycletotal = 0; // XXX For now we will not pay attention to this reset sequence

    set(1, "_reset");
    emit runStopped();
}

/*
 * Advance the simulation by one half-cycle of the clock
 */
inline void ClassSimX::halfCycle()
{
    pin_t clk = ! readBit("clk");
    if (clk) // Before the clock rise, service the chip pins
    {
        bool m1   = readBit("_m1");
        bool rfsh = readBit("_rfsh");
        bool mreq = readBit("_mreq");
        bool rd   = readBit("_rd");
        bool wr   = readBit("_wr");
        bool iorq = readBit("_iorq");

        if (!m1 && rfsh && !mreq && !rd &&  wr &&  iorq && readBit("t2"))
            handleMemRead(readAB()); // Instruction read
        else
        if ( m1 && rfsh && !mreq && !rd &&  wr &&  iorq && readBit("t3"))
            handleMemRead(readAB()); // Data read
        else
        if ( m1 && rfsh && !mreq &&  rd && !wr &&  iorq && readBit("t3"))
            handleMemWrite(readAB()); // Data write
        else
        if ( m1 && rfsh &&  mreq && !rd &&  wr && !iorq && readBit("t3"))
            handleIORead(readAB()); // IO read
        else
        if ( m1 && rfsh &&  mreq &&  rd && !wr && !iorq && readBit("t3"))
            handleIOWrite(readAB()); // IO write
        else
        if (!m1 && rfsh &&  mreq &&  rd &&  wr && !iorq)
            handleIrq(readAB()); // Interrupt request/Ack cycle
    }
    set(clk, "clk"); // Let the clock edge propagate through the chip

    // After each half-cycle, populate the watch data
    int it;
    watch *w = ::controller.getWatch().getFirst(it);
    while (w != nullptr)
    {
        net_t bit = w->enabled ? readBit(w->name) : 3;
        ::controller.getWatch().append(w, m_hcycletotal, bit);

        w = ::controller.getWatch().getNext(it);
    }

    m_hcyclecnt++; // Half-cycle count for this single simulation run
    m_hcycletotal++; // Total cycle count since the chip reset
}

inline void ClassSimX::handleMemRead(uint16_t ab)
{
    uint8_t db = ::controller.readMem(ab);
    setDB(db);
}

inline void ClassSimX::handleMemWrite(uint16_t ab)
{
    uint8_t db = readByte("db");
    ::controller.writeMem(ab, db);
}

inline void ClassSimX::handleIORead(uint16_t ab)
{
    uint8_t db = ::controller.readIO(ab);
    setDB(db);
}

inline void ClassSimX::handleIOWrite(uint16_t ab)
{
    uint8_t db = readByte("db");
    ::controller.writeIO(ab, db);
}

inline void ClassSimX::handleIrq(uint16_t ab)
{
    uint8_t db = ::controller.readIO(ab);
    setDB(db);
}

inline void ClassSimX::setDB(uint8_t db)
{
    for (int i=0; i < 8; i++, db >>= 1)
        set(db & 1, "db" % QString::number(i));
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

inline bool ClassSimX::getNetValue()
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

inline void ClassSimX::recalcNetlist(QVector<net_t> list)
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

inline void ClassSimX::recalcNet(net_t n)
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

inline void ClassSimX::setTransOn(class trans &t)
{
    if (t.on) return;
    t.on = true;
    addRecalcNet(t.c1);
}

inline void ClassSimX::setTransOff(class trans &t)
{
    if (!t.on) return;
    t.on = false;
    addRecalcNet(t.c1);
    addRecalcNet(t.c2);
}

inline void ClassSimX::addRecalcNet(net_t n)
{
    if ((n==ngnd) || (n==npwr)) return;
    if (!recalcList.contains(n))
        recalcList.append(n);
}

inline void ClassSimX::getNetGroup(net_t n)
{
    group.clear();
    addNetToGroup(n);
}

inline void ClassSimX::addNetToGroup(net_t n)
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
    qInfo() << "Loading simX resources from" << dir;
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
                    if (!m_netnames.contains(key))
                        m_netnames[key] = list[1].toUInt();
                    else
                        qWarning() << "Duplicate key" << key;
                }
                else
                    qWarning() << "Invalid line" << list;
            }
            else
                qDebug() << "Skipping" << line;
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
                    Q_ASSERT(i < MAX_TRANSDEFS);
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
        QString bit_name = "ab" % QString::number(i);
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
        QString bit_name = name % QString::number(i);
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
void ClassSimX::readState(z80state &z)
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
        z._db[i] = readPin("db" % QString::number(i));
    z.clk = readPin("clk");
    z.intr = readPin("int");
    z.nmi = readPin("nmi");
    z.halt = readPin("_halt");
    z.mreq = readPin("_mreq");
    z.iorq = readPin("_iorq");
    z.rd = readPin("_rd");
    z.wr = readPin("_wr");
    z.busak = readPin("_busak");
    z.wait= readPin("wait");
    z.busrq = readPin("busrq");
    z.reset = readPin("_reset");
    z.m1 = readPin("_m1");
    z.rfsh = readPin("_rfsh");
    for (int i=0; i<6; i++)
    {
        z.m[i] = readPin("m" % QString::number(i+1));
        z.t[i] = readPin("t" % QString::number(i+1));
    }
    z.instr = readByte("_instr");
}
