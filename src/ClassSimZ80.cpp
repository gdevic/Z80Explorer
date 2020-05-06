#include "ClassSimZ80.h"
#include "ClassController.h"
#include <QDebug>
#include <QStringBuilder>
#include <QtConcurrent>

ClassSimZ80::ClassSimZ80()
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &ClassSimZ80::onTimeout);
}

/*
 * One-time initialization
 */
bool ClassSimZ80::initChip()
{
    Q_ASSERT(ngnd == 1);
    Q_ASSERT(npwr == 2);

    // Initialize GND and Vcc
    m_netlist[ngnd].state = false;
    m_netlist[npwr].state = true;

    // Initialize nets which can float, have hi-Z value, on a readout
    net_t mreq = get("_mreq");
    net_t iorq = get("_iorq");
    net_t rd = get("_rd");
    net_t wr = get("_wr");
    net_t ab0 = get("ab0"); // Sanity check that we have address bus name
    net_t db0 = get("db0"); // Sanity check that we have data bus name
    if (mreq && iorq && rd && wr && ab0 && db0)
    {
        m_netlist[mreq].floats = true;
        m_netlist[iorq].floats = true;
        m_netlist[rd].floats = true;
        m_netlist[wr].floats = true;
        for (int i=0; i<16; i++)
            m_netlist[get(QString("ab%1").arg(i))].floats = true;
#if DATA_PINS_HI_Z
        // It turns out data pins very rarely drive the data bus: most of the time they are in hi-Z
        // (or input mode). Uncomment this to see DB tristated unless actively driving a value out.
        for (int i=0; i<8; i++)
            m_netlist[get(QString("db%1").arg(i))].floats = true;
#endif
    }
    else
    {
        qCritical() << "Unknown net name _mreq,_iorq,_rd,_wr,ab0 or db0";
        return false;
    }

    // Turn off all transistors
    for (auto &t : m_transdefs)
        t.on = false;

    return true;
}

// XXX Remove this timer here and implement it somewhere else (?)
void ClassSimZ80::onTimeout()
{
    qreal hz = (m_hcyclecnt / 2.0) / (m_elapsed.elapsed() / 1000.0);
    qDebug() << "Half-Cycles:" << m_hcycletotal << "~" << ((hz < 100) ? 0 : hz) << " Hz";
    if (m_runcount <= 0)
        m_timer.stop();
}

/*
 * Sets an input pin to a value, return false if the index specified undefined input pin
 */
bool ClassSimZ80::setPin(uint index, pin_t p)
{
    const static QStringList pins = { "_int", "_nmi", "_busrq", "_wait", "_reset" };
    if (index < uint(pins.count()))
    {
        set(p, pins[index]);
        return true;
    }
    Q_ASSERT(0);
    return false;
}

/*
 * Run the simulation for the given number of clocks. Zero stops the simulation.
 */
void ClassSimZ80::doRunsim(uint ticks)
{
    if (!m_runcount && !ticks) // For Stop signal (ticks=0), do nothing if the sim thread is not running
        return;
    if (m_runcount) // If the sim thread is already running, simply set the new tick count limiter
        m_runcount = ticks;
    else
    {
        if (ticks < 3) // Optimize for special cases of up to 2 half-cycle steps, makes the interaction more responsive
        {
            while (ticks--)
                halfCycle();
            emit runStopped(m_hcycletotal);
            onTimeout(); // XXX Can we get rid of this chain?
        }
        else // If the sim thread is not running, start it and set the tick count limiter
        {
            m_runcount = ticks;
            m_timer.start();
            m_elapsed.start();
            m_hcyclecnt = 0;
            // Code in this block will run in another thread
            QFuture<void> future = QtConcurrent::run([=]()
            {
                while (m_runcount.fetchAndAddOrdered(-1) > 0)
                    halfCycle();
                m_runcount = 0;
                emit runStopped(m_hcycletotal);
            });
        }
    }
}

/*
 * Run chip reset sequence, returns the total number of cycles the reset took
 */
uint ClassSimZ80::doReset()
{
    // If the simulation is running, stop it instead
    if (m_runcount)
    {
        m_runcount = 0;
        return 0;
    }

    // Initialize control pins
    set(0, "_reset");
    set(1, "clk");
    set(1, "_busrq");
    set(1, "_int");
    set(1, "_nmi");
    set(1, "_wait");
#if USE_MY_LISTS
    allNets();
    recalcNetlist();
#else
    QVector<net_t> list = allNets();
    recalcNetlist(list);
#endif

    // Start the cycle count from the begining of a reset sequence
    m_hcycletotal = 0;

    // Propagate the reset before deasserting it
    for (int i=0; i < 8; i++)
        halfCycle();

    set(1, "_reset");

    return m_hcycletotal;
}

/*
 * Advance the simulation by one half-cycle of the clock
 */
inline void ClassSimZ80::halfCycle()
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

    ::controller.onTick(m_hcycletotal);

    set(clk, "clk"); // Let the clock edge propagate through the chip

    // After each half-cycle, populate the watch data
    int it;
    watch *w = ::controller.getWatch().getFirst(it);
    while (w != nullptr)
    {
        pin_t bit = readBit(w->name);
        ::controller.getWatch().append(w, m_hcycletotal, bit);

        w = ::controller.getWatch().getNext(it);
    }

    m_hcyclecnt.fetchAndAddRelaxed(1); // Half-cycle count for this single simulation run
    m_hcycletotal.fetchAndAddRelaxed(1); // Total half-cycle count since the chip reset
}

inline void ClassSimZ80::handleMemRead(uint16_t ab)
{
    uint8_t db = ::controller.readMem(ab);
    setDB(db);
}

inline void ClassSimZ80::handleMemWrite(uint16_t ab)
{
    uint8_t db = readByte("db");
    ::controller.writeMem(ab, db);
}

inline void ClassSimZ80::handleIORead(uint16_t ab)
{
    uint8_t db = ::controller.readIO(ab);
    setDB(db);
}

inline void ClassSimZ80::handleIOWrite(uint16_t ab)
{
    uint8_t db = readByte("db");
    ::controller.writeIO(ab, db);
}

inline void ClassSimZ80::handleIrq(uint16_t ab)
{
    uint8_t db = ::controller.readIO(ab);
    setDB(db);
}

inline void ClassSimZ80::setDB(uint8_t db)
{
    for (int i=0; i < 8; i++, db >>= 1)
        set(db & 1, "db" % QString::number(i));
}

/*
 * Sets a named input net to pullup or pulldown status
 */
inline void ClassSimZ80::set(bool on, QString name)
{
    net_t n = get(name);
    m_netlist[n].pullup = on;
    m_netlist[n].pulldown = !on;
#if USE_MY_LISTS
    m_list[0] = n;
    m_listIndex = 1;
    recalcNetlist();
#else
    QVector<net_t> list {n};
    recalcNetlist(list);
#endif
}

#if USE_MY_LISTS
inline bool ClassSimZ80::getNetValue()
{
    // 1. deal with power connections first
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
        if (*p == ngnd) return false;
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
        if (*p == npwr) return true;
    // 2. deal with pullup/pulldowns next
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
    {
        net &net = m_netlist[*p];
        if (net.pullup) return true;
        if (net.pulldown) return false;
    }
    // 3. resolve connected set of floating nodes
    // based on state of largest (by #connections) node
    // (previously this was any node with state true wins)
    auto max_state = false;
    auto max_conn = 0;
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
    {
        net &net = m_netlist[*p];
        auto conn = net.gates.count() + net.c1c2s.count();
        if (conn > max_conn)
        {
            max_conn = conn;
            max_state = net.state;
        }
    }
    return max_state;
}
#else
inline bool ClassSimZ80::getNetValue()
{
    // 1. deal with power connections first
    if (Q_UNLIKELY(group.contains(ngnd))) return false;
    if (Q_UNLIKELY(group.contains(npwr))) return true;
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
#endif

#if EARLY_LOOP_DETECTION
#if USE_MY_LISTS
inline void ClassSimZ80::recalcNetlist()
{
    m_recalcListIndex = 0;
    int t01rep = 3;
    while(m_listIndex)
    {
        t01opt = 0;
        for (int i=0; i<m_listIndex; i++)
            recalcNet(m_list[i]);
        memcpy(m_list, m_recalcList, m_recalcListIndex * sizeof (net_t));
        m_listIndex = m_recalcListIndex;
        m_recalcListIndex = 0;
        // Optimization: if no transistors changed state, or if the *same group* of transistors toggled on and off,
        // which happens when there is a feedback loop, exit.
        // This is not necessarily 100% reliable since different trans id's may still add up to the same value,
        // so an additional counter, t01opt, activates it only after the early loop detects 3 times in a row.
        if ((t01opt == 0) && (t01rep-- == 0))
            break;
        if (t01opt != 0)
            t01rep = 3;
    }
}
#else
inline void ClassSimZ80::recalcNetlist(QVector<net_t> &list)
{
    recalcList.clear();
    int t01rep = 3;
    while(list.count())
    {
        t01opt = 0;
        for (auto n : list)
            recalcNet(n);
        list = recalcList;
        recalcList.clear();
        // Optimization: if no transistors changed state, or if the *same group* of transistors toggled on and off,
        // which happens when there is a feedback loop, exit.
        // This is not necessarily 100% reliable since different trans id's may still add up to the same value,
        // so an additional counter, t01opt, activates it only after the early loop detects 3 times in a row.
        if ((t01opt == 0) && (t01rep-- == 0))
            break;
        if (t01opt != 0)
            t01rep = 3;
    }
}
#endif
#else // Legacy version
#if USE_MY_LISTS
inline void ClassSimZ80::recalcNetlist()
{
    m_recalcListIndex = 0;
    for (int i=0; i<50 && m_listIndex; i++) // loop limiter
    {
        for (int i=0; i<m_listIndex; i++)
            recalcNet(m_list[i]);
        memcpy(m_list, m_recalcList, m_recalcListIndex * sizeof (net_t));
        m_listIndex = m_recalcListIndex;
        m_recalcListIndex = 0;
    }
}
#else // USE_MY_LISTS
inline void ClassSimZ80::recalcNetlist(QVector<net_t> &list)
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
#endif // USE_MY_LISTS
#endif

#if USE_MY_LISTS
inline void ClassSimZ80::recalcNet(net_t n)
{
    if (Q_UNLIKELY((n==ngnd) || (n==npwr))) return;
    getNetGroup(n);
    bool newState = getNetValue();
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
    {
        net &net = m_netlist[*p];
        if (net.state == newState) continue;
        net.state = newState;
        for (int i=0; i<net.gates.count(); i++)
        {
            if (net.state)
                setTransOn(net.gates[i]);
            else
                setTransOff(net.gates[i]);
        }
    }
}
#else
inline void ClassSimZ80::recalcNet(net_t n)
{
    if (Q_UNLIKELY((n==ngnd) || (n==npwr))) return;
    getNetGroup(n);
    bool newState = getNetValue();
    for (auto i : group)
    {
        net &net = m_netlist[i];
        if (net.state == newState) continue;
        net.state = newState;
        for (int i=0; i<net.gates.count(); i++)
        {
            if (net.state)
                setTransOn(net.gates[i]);
            else
                setTransOff(net.gates[i]);
        }
    }
}
#endif

#if USE_MY_LISTS
void ClassSimZ80::allNets()
{
    m_listIndex = 0;
    for (net_t n=0; n < m_netlist.count(); n++)
    {
        if ((n==ngnd) || (n==npwr) || (m_netlist[n].gates.count()==0 && m_netlist[n].c1c2s.count()==0))
            continue;
        m_list[m_listIndex++] = n;
    }
}
#else
QVector<net_t> ClassSimZ80::allNets()
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
#endif

inline void ClassSimZ80::setTransOn(struct trans *t)
{
    if (t->on) return;
#if EARLY_LOOP_DETECTION
    t01opt += t->id;
#endif
    t->on = true;
    addRecalcNet(t->c1);
}

inline void ClassSimZ80::setTransOff(struct trans *t)
{
    if (!t->on) return;
#if EARLY_LOOP_DETECTION
    t01opt -= t->id;
#endif
    t->on = false;
    addRecalcNet(t->c1);
    addRecalcNet(t->c2);
}

#if USE_MY_LISTS
inline void ClassSimZ80::addRecalcNet(net_t n)
{
    if (Q_UNLIKELY((n==ngnd) || (n==npwr))) return;
    for (net_t *p = m_recalcList; p < (m_recalcList + m_recalcListIndex); p++)
        if (*p == n)
            return;
    m_recalcList[m_recalcListIndex++] = n;
}
#else
inline void ClassSimZ80::addRecalcNet(net_t n)
{
    if (Q_UNLIKELY((n==ngnd) || (n==npwr))) return;
    if (!recalcList.contains(n))
        recalcList.append(n);
}
#endif

#if USE_MY_LISTS
inline void ClassSimZ80::getNetGroup(net_t n)
{
    m_groupIndex = 0;
    addNetToGroup(n);
}
#else
inline void ClassSimZ80::getNetGroup(net_t n)
{
    group.clear();
    addNetToGroup(n);
}
#endif

#if USE_MY_LISTS
inline void ClassSimZ80::addNetToGroup(net_t n)
{
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
        if (*p == n)
            return;
    m_group[m_groupIndex++] = n;
    if (Q_UNLIKELY((n==ngnd) || (n==npwr))) return;
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
#else
inline void ClassSimZ80::addNetToGroup(net_t n)
{
    if (group.contains(n)) return;
    group.append(n);
    if (Q_UNLIKELY((n==ngnd) || (n==npwr))) return;
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
#endif

/*
 * Reads chip state into a state structure
 */
void ClassSimZ80::readState(z80state &z)
{
    z.ab = readAB();
    z.db = readByte("db");

    z.ab0 = readBit("ab0");
    z.db0 = readBit("db0");
    z.mreq = readBit("_mreq");
    z.iorq = readBit("_iorq");
    z.rd = readBit("_rd");
    z.wr = readBit("_wr");

    z.busak = readBit("_busak");
    z.busrq = readBit("_busrq");
    z.clk = readBit("clk");
    z.halt = readBit("_halt");
    z.intr = readBit("_int");
    z.m1 = readBit("_m1");
    z.nmi = readBit("_nmi");
    z.reset = readBit("_reset");
    z.rfsh = readBit("_rfsh");
    z.wait= readBit("_wait");

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

    for (int i=0; i<6; i++)
    {
        z.m[i] = readBit("m" % QString::number(i+1));
        z.t[i] = readBit("t" % QString::number(i+1));
    }
    z.instr = readByte("_instr");
}
