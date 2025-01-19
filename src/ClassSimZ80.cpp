#include "ClassController.h"
#include "ClassSimZ80.h"
#include <QDebug>
#include <QStringBuilder>
#include <QtConcurrent>

ClassSimZ80::ClassSimZ80()
{
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

        // XXX This should go into some kind of init file for this cpu
        // We should also watch those 3 internal buses for hi-Z
        m_netlist[get("dbus0")].floats = true;
        m_netlist[get("ubus0")].floats = true;
        m_netlist[get("vbus0")].floats = true;

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


/*
 * Simulation heartbeat timer (500ms)
 */
void ClassSimZ80::onTimeout()
{
    // Estimate simulated frequency of the running netlist
    m_estHz = (uint(m_hcyclecnt) / 2.0) / (m_elapsed.elapsed() / 1000.0);
    if (m_runcount <= 0)
        m_timer.stop();
    else
        emit ::controller.onRunHeartbeat(m_hcyclecnt);
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
        emit ::controller.onRunStarting(ticks);
        if (ticks < 3) // Optimize for special cases of up to 2 half-cycle steps, makes the interaction more responsive
        {
            while (ticks--)
                halfCycle();
            emit ::controller.onRunStopped(m_hcycletotal);
        }
        else // If the sim thread is not running, start it and set the tick count limiter
        {
            m_runcount = ticks;
            m_timer.start(500);
            m_elapsed.start();
            m_hcyclecnt = 0;
            // Code in this block will run in another thread
            QFuture<void> future = QtConcurrent::run([=]()
            {
                while (m_runcount.fetchAndAddOrdered(-1) > 0)
                    halfCycle();
                m_runcount = 0;
                emit ::controller.onRunStopped(m_hcycletotal);
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
#if USE_PERFORMANCE_SIM
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
    if (clk && readBit("_rfsh")) // Before the clock rise, service the chip pins (unless it is a refresh cycle)
    {
        bool m1   = readBit("_m1");
        bool rfsh = 1; //readBit("_rfsh");
        bool mreq = readBit("_mreq");
        bool rd   = readBit("_rd");
        bool wr   = readBit("_wr");
        bool iorq = readBit("_iorq");
        bool t2   = readBit("t2");
        bool t3   = readBit("t3");

        if (!m1 && rfsh && !mreq && !rd &&  wr &&  iorq && t2)
            handleMemRead(readAB()); // Instruction read
        else
        if ( m1 && rfsh && !mreq && !rd &&  wr &&  iorq && t3)
            handleMemRead(readAB()); // Data read
        else
        if ( m1 && rfsh && !mreq &&  rd && !wr &&  iorq && t3)
            handleMemWrite(readAB()); // Data write
        else
        if ( m1 && rfsh &&  mreq && !rd &&  wr && !iorq && t3)
            handleIORead(readAB()); // IO read
        else
        if ( m1 && rfsh &&  mreq &&  rd && !wr && !iorq && t3)
            handleIOWrite(readAB()); // IO write
        else
        if (!m1 && rfsh &&  mreq &&  rd &&  wr && !iorq)
            handleIrq(); // Interrupt request/Ack cycle
    }

    set(clk, "clk"); // Let the clock edge propagate through the chip

    // After each half-cycle, populate the watch data
    if (::controller.getWatch().getWatchlistLen()) // Removing all watches increases the performance
    {
        int it;
        watch *w = ::controller.getWatch().getFirst(it);
        while (w != nullptr)
        {
            pin_t bit = readBit(w->name);
            ::controller.getWatch().append(w, m_hcycletotal, bit);

            w = ::controller.getWatch().getNext(it);
        }
    }

    // Inform the rest of the app that the clock half-tick happened
    ::controller.onTick(m_hcycletotal);

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

inline void ClassSimZ80::handleIrq()
{
    // IO address 0x81 holds the value to be shown on the bus
    uint8_t db = ::controller.readIO(0x81);
    setDB(db);
}

inline void ClassSimZ80::setDB(uint8_t db)
{
    set(db &   1, "db0");
    set(db &   2, "db1");
    set(db &   4, "db2");
    set(db &   8, "db3");
    set(db &  16, "db4");
    set(db &  32, "db5");
    set(db &  64, "db6");
    set(db & 128, "db7");
}

/*
 * Sets a named input net to pullup or pulldown status
 */
inline void ClassSimZ80::set(bool on, QString name)
{
    net_t n = get(name);
    if (m_netlist[n].isHigh == on) // The state did not change
        return;
    m_netlist[n].isHigh = on;
    m_netlist[n].isLow = !on;
#if USE_PERFORMANCE_SIM
    m_list[0] = n;
    m_listIndex = 1;
    recalcNetlist();
#else
    QVector<net_t> list {n};
    recalcNetlist(list);
#endif
}

#if USE_PERFORMANCE_SIM
inline bool ClassSimZ80::getNetValue()
{
    // 1. Deal with power connections first
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
        if (*p == ngnd) return false;
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
        if (*p == npwr) return true;
    // 2. Deal with pullup/pulldowns next
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
    {
        Net &net = m_netlist[*p];
        if (net.isHigh) return true;
        if (net.isLow) return false;
    }
    // 3. Resolve connected set of floating nodes
    // Several approaches work:
    // - based on state of largest (by #connections) node
    // - that, either by the number of connected gates, or by the number of connected pins
    // - any node for which state is true
    auto max_state = false;
    //auto max_conn = 0;
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
    {
        Net &net = m_netlist[*p];
#if 0
        // We just want to pick the larger, or "stronger" net, assuming that one would have more transistor connections
        //auto conn = net.gates.count() + net.c1c2s.count();
        //auto conn = net.c1c2s.count();
        auto conn = net.gates.count();
        if (conn > max_conn)
        {
            max_conn = conn;
            max_state = net.state;
        }
#endif
        // Or we simply pick the first node which state is high
        if (net.state == true)
            return true;
    }
    return max_state;
}
#else
inline bool ClassSimZ80::getNetValue()
{
    // 1. Deal with power connections first
    if (Q_UNLIKELY(group.contains(ngnd))) return false;
    if (Q_UNLIKELY(group.contains(npwr))) return true;
    // 2. Deal with pullup/pulldowns next
    for (auto i : group)
    {
        auto net = m_netlist[i];
        if (net.isHigh) return true;
        if (net.isLow) return false;
    }
    // 3. Resolve connected set of floating nodes
    // Several approaches work:
    // - based on state of largest (by #connections) node
    // - that, either by the number of connected gates, or by the number of connected pins
    // - any node for which state is true
    auto max_state = false;
    auto max_conn = 0;
    for (auto i : group)
    {
        auto net = m_netlist[i];
#if 0
        // We just want to pick the larger, or "stronger" net, assuming that one would have more transistor connections
        //auto conn = net.gates.count() + net.c1c2s.count();
        auto conn = net.c1c2s.count();
        auto conn = net.gates.count();
        if (conn > max_conn)
        {
            max_conn = conn;
            max_state = net.state;
        }
#endif
        // Or we simply pick the first node which state is high
        if (net.state == true)
            return true;
    }
    return max_state;
}
#endif

#if USE_PERFORMANCE_SIM
inline void ClassSimZ80::recalcNetlist()
{
    m_recalcListIndex = 0;
    while(m_listIndex)
    {
        for (int i=0; i<m_listIndex; i++)
            recalcNet(m_list[i]);

        // This performance code path depends on this check since it does not have any other loop limiter. The early exit check below is tightly tied to this specific
        // Z80 netlist and the order of nets and transistors, and will detect the only case when the nets are toggling endlessly.
        //if ((m_recalcListIndex == 32) && (m_recalcList[0] == 791) && (m_recalcList[1] == 703) && (m_recalcList[2] == 907) && (m_recalcList[3] == 713) && (m_recalcList[4] == 917) && (m_recalcList[5] == 798) && (m_recalcList[6] == 922) && (m_recalcList[7] == 806) && (m_recalcList[8] == 933) && (m_recalcList[9] == 714) && (m_recalcList[10] == 810) && (m_recalcList[11] == 936) && (m_recalcList[12] == 731) && (m_recalcList[13] == 953) && (m_recalcList[14] == 841) && (m_recalcList[15] == 958) && (m_recalcList[16] == 846) && (m_recalcList[17] == 739) && (m_recalcList[18] == 969) && (m_recalcList[19] == 863) && (m_recalcList[20] == 749) && (m_recalcList[21] == 974) && (m_recalcList[22] == 982) && (m_recalcList[23] == 871) && (m_recalcList[24] == 752) && (m_recalcList[25] == 984) && (m_recalcList[26] == 884) && (m_recalcList[27] == 998) && (m_recalcList[28] == 774) && (m_recalcList[29] == 885) && (m_recalcList[30] == 901) && (m_recalcList[31] == 777))
        // Empirically found that it is necessary to only check the first one, when 32 nets are on the list
        if ((m_recalcListIndex == 32) && (m_recalcList[0] == 791))
            break;

        memcpy(m_list, m_recalcList, m_recalcListIndex * sizeof (net_t));
        m_listIndex = m_recalcListIndex;
        m_recalcListIndex = 0;
    }
}
#else
inline void ClassSimZ80::recalcNetlist(QVector<net_t> &list)
{
    recalcList.clear();
    for (int i=0; i<100 && list.count(); i++) // loop limiter
    {
        // This strictly a performance improvement complements the loop limiter. The early exit check below is tightly tied to this specific
        // Z80 netlist and the order of nets and transistors, and will detect the only case when the nets are toggling endlessly.
        if (list.count() == 32) // There are 32 nets that are unstable, exit early when the list contains only those
        {
            //if ((list[0] == 791) && (list[1] == 703) && (list[2] == 907) && (list[3] == 713) && (list[4] == 917) && (list[5] == 798) && (list[6] == 922) && (list[7] == 806) && (list[8] == 933) && (list[9] == 714) && (list[10] == 810) && (list[11] == 936) && (list[12] == 731) && (list[13] == 953) && (list[14] == 841) && (list[15] == 958) && (list[16] == 846) && (list[17] == 739) && (list[18] == 969) && (list[19] == 863) && (list[20] == 749) && (list[21] == 974) && (list[22] == 982) && (list[23] == 871) && (list[24] == 752) && (list[25] == 984) && (list[26] == 884) && (list[27] == 998) && (list[28] == 774) && (list[29] == 885) && (list[30] == 901) && (list[31] == 777))
            // Empirically found that it is necessary to only check the first one, when 32 nets are on the list
            if (list[0] == 791)
                break;
        }

        for (auto n : list)
            recalcNet(n);
        list = recalcList;
        recalcList.clear();
    }
}
#endif

#if USE_PERFORMANCE_SIM
inline void ClassSimZ80::recalcNet(net_t n)
{
    if (Q_UNLIKELY((n==ngnd) || (n==npwr))) return;
    getNetGroup(n);
    bool newState = getNetValue();
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
    {
        Net &net = m_netlist[*p];
        if (net.state == newState) continue;
        net.state = newState;

        if (net.state)
            for (Trans *t : net.gates)
            {
                if (!t->on)
                {
                    t->on = true;
                    addRecalcNet(t->c1);
                }
            }
        else
            for (Trans *t : net.gates)
            {
                if (t->on)
                {
                    t->on = false;
                    addRecalcNet(t->c1);
                    addRecalcNet(t->c2);
                }
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
        Net &net = m_netlist[i];
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

inline void ClassSimZ80::setTransOn(struct Trans* t)
{
    if (t->on) return;
    t->on = true;
    addRecalcNet(t->c1);
}

inline void ClassSimZ80::setTransOff(struct Trans* t)
{
    if (!t->on) return;
    t->on = false;
    addRecalcNet(t->c1);
    addRecalcNet(t->c2);
}
#endif

#if USE_PERFORMANCE_SIM
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

#if USE_PERFORMANCE_SIM
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

#if USE_PERFORMANCE_SIM
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

#if USE_PERFORMANCE_SIM
inline void ClassSimZ80::addNetToGroup(net_t n)
{
    for (net_t *p = m_group; p < (m_group + m_groupIndex); p++)
        if (*p == n)
            return;
    m_group[m_groupIndex++] = n;
    if (Q_UNLIKELY((n==ngnd) || (n==npwr))) return;
    for (auto &t : m_netlist[n].c1c2s)
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
    for (auto &t : m_netlist[n].c1c2s)
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

    z.instr = readByte("instr");
    z.nED = readBit(265); // Decode ED
    z.nCB = readBit(263); // Decode CB
}

const QString z80state::decode[256] {
    "nop","ld bc,nn","ld (bc),a","inc bc","inc b","dec b","ld b,n","rlca","ex af,af'","add hl,bc","ld a,(bc)","dec bc","inc c","dec c","ld c,n","rrca",
    "djnz n","ld de,nn","ld (de),a","inc de","inc d","dec d","ld d,n","rla","jr n","add hl,de","ld a,(de)","dec de","inc e","dec e","ld e,n","rra",
    "jr nz,n","ld hl,nn","ld (nn),hl","inc hl","inc h","dec h","ld h,n","daa","jr z,n","add hl,hl","ld hl,(nn)","dec hl","inc l","dec l","ld l,n","cpl",
    "jr nc,n","ld sp,nn","ld (nn),a","inc sp","inc (hl)","dec (hl)","ld (hl),n","scf","jr c,n","add hl,sp","ld a,(nn)","dec sp","inc a","dec a","ld a,n","ccf",
    "ld b,b","ld b,c","ld b,d","ld b,e","ld b,h","ld b,l","ld b,(hl)","ld b,a","ld c,b","ld c,c","ld c,d","ld c,e","ld c,h","ld c,l","ld c,(hl)","ld c,a",
    "ld d,b","ld d,c","ld d,d","ld d,e","ld d,h","ld d,l","ld d,(hl)","ld d,a","ld e,b","ld e,c","ld e,d","ld e,e","ld e,h","ld e,l","ld e,(hl)","ld e,a",
    "ld h,b","ld h,c","ld h,d","ld h,e","ld h,h","ld h,l","ld h,(hl)","ld h,a","ld l,b","ld l,c","ld l,d","ld l,e","ld l,h","ld l,l","ld l,(hl)","ld l,a",
    "ld (hl),b","ld (hl),c","ld (hl),d","ld (hl),e","ld (hl),h","ld (hl),l","halt","ld (hl),a","ld a,b","ld a,c","ld a,d","ld a,e","ld a,h","ld a,l","ld a,(hl)","ld a,a",
    "add a,b","add a,c","add a,d","add a,e","add a,h","add a,l","add a,(hl)","add a,a","adc a,b","adc a,c","adc a,d","adc a,e","adc a,h","adc a,l","adc a,(hl)","adc a,a",
    "sub b","sub c","sub d","sub e","sub h","sub l","sub (hl)","sub a","sbc a,b","sbc a,c","sbc a,d","sbc a,e","sbc a,h","sbc a,l","sbc a,(hl)","sbc a,a",
    "and b","and c","and d","and e","and h","and l","and (hl)","and a","xor b","xor c","xor d","xor e","xor h","xor l","xor (hl)","xor a",
    "or b","or c","or d","or e","or h","or l","or (hl)","or a","cp b","cp c","cp d","cp e","cp h","cp l","cp (hl)","cp a",
    "ret nz","pop bc","jp nz,nn","jp nn","call nz,nn","push bc","add a,n","rst 00h","ret z","ret","jp z,nn","CB  ","call z,nn","call nn","adc a,n","rst 08h",
    "ret nc","pop de","jp nc,nn","out (n),a","call nc,nn","push de","sub n","rst 10h","ret c","exx","jp c,nn","in a,(n)","call c,nn","DD (IX)","sbc a,n","rst 18h",
    "ret po","pop hl","jp po,nn","ex (sp),hl","call po,nn","push hl","and n","rst 20h","ret pe","jp (hl)","jp pe,nn","ex de,hl","call pe,nn","ED","xor n","rst 28h",
    "ret p","pop af","jp p,nn","di","call p,nn","push af","or n","rst 30h","ret m","ld sp,hl","jp m,nn","ei","call m,nn","FD (IY)","cp n","rst 38h"};

const QString z80state::decodeED[256] {
    "","","","","","","","","","","","","","","","",
    "","","","","","","","","","","","","","","","",
    "","","","","","","","","","","","","","","","",
    "","","","","","","","","","","","","","","","",
    "in b,(c)","out (c),b","sbc hl,bc","ld (nn),bc","neg","retn","im 0","ld i,a","in c,(c)","out (c),c","adc hl,bc","ld bc,(nn)","neg","reti","im 0","ld r,a",
    "in d,(c)","out (c),d","sbc hl,de","ld (nn),de","neg","retn","im 1","ld a,i","in e,(c)","out (c),e","adc hl,de","ld de,(nn)","neg","retn","im 2","ld a,r",
    "in h,(c)","out (c),h","sbc hl,hl","ld (nn),hl","neg","retn","im 0","rrd","in l,(c)","out (c),l","adc hl,hl","ld hl,(nn)","neg","retn","im 0","rld",
    "in (c)","out (c),0","sbc hl,sp","ld (nn),sp","neg","retn","im 1","","in a,(c)","out (c),a","adc hl,sp","ld sp,(nn)","neg","retn","im 2","",
    "","","","","","","","","","","","","","","","",
    "","","","","","","","","","","","","","","","",
    "ldi","cpi","ini","outi","","","","",
    "ldd","cpd","ind","outd","","","","",
    "ldir","cpir","inir","otir","","","","",
    "lddr","cpdr","indr","outdr","","","","",
    "","","","","","","","","","","","","","","","",
    "","","","","","","","","","","","","","","","",
    "","","","","","","","","","","","","","","","",
    "","","","","","","","","","","","","","","","" };

const QString z80state::decodeCB[256] {
    "rlc b","rlc c","rlc d","rlc e","rlc h","rlc l","rlc (hl)","rlc a","rrc b","rrc c","rrc d","rrc e","rrc h","rrc l","rrc (hl)","rrc a",
    "rl b","rl c","rl d","rl e","rl h","rl l","rl (hl)","rl a","rr b","rr c","rr d","rr e","rr h","rr l","rr (hl)","rr a",
    "sla b","sla c","sla d","sla e","sla h","sla l","sla (hl)","sla a","sra b","sra c","sra d","sra e","sra h","sra l","sra (hl)","sra a",
    "sll b","sll c","sll d","sll e","sll h","sll l","sll (hl)","sll a","srl b","srl c","srl d","srl e","srl h","srl l","srl (hl)","srl a",
    "bit 0,b","bit 0,c","bit 0,d","bit 0,e","bit 0,h","bit 0,l","bit 0,(hl)","bit 0,a","bit 1,b","bit 1,c","bit 1,d","bit 1,e","bit 1,h","bit 1,l","bit 1,(hl)","bit 1,a",
    "bit 2,b","bit 2,c","bit 2,d","bit 2,e","bit 2,h","bit 2,l","bit 2,(hl)","bit 2,a","bit 3,b","bit 3,c","bit 3,d","bit 3,e","bit 3,h","bit 3,l","bit 3,(hl)","bit 3,a",
    "bit 4,b","bit 4,c","bit 4,d","bit 4,e","bit 4,h","bit 4,l","bit 4,(hl)","bit 4,a","bit 5,b","bit 5,c","bit 5,d","bit 5,e","bit 5,h","bit 5,l","bit 5,(hl)","bit 5,a",
    "bit 6,b","bit 6,c","bit 6,d","bit 6,e","bit 6,h","bit 6,l","bit 6,(hl)","bit 6,a","bit 7,b","bit 7,c","bit 7,d","bit 7,e","bit 7,h","bit 7,l","bit 7,(hl)","bit 7,a",
    "res 0,b","res 0,c","res 0,d","res 0,e","res 0,h","res 0,l","res 0,(hl)","res 0,a","res 1,b","res 1,c","res 1,d","res 1,e","res 1,h","res 1,l","res 1,(hl)","res 1,a",
    "res 2,b","res 2,c","res 2,d","res 2,e","res 2,h","res 2,l","res 2,(hl)","res 2,a","res 3,b","res 3,c","res 3,d","res 3,e","res 3,h","res 3,l","res 3,(hl)","res 3,a",
    "res 4,b","res 4,c","res 4,d","res 4,e","res 4,h","res 4,l","res 4,(hl)","res 4,a","res 5,b","res 5,c","res 5,d","res 5,e","res 5,h","res 5,l","res 5,(hl)","res 5,a",
    "res 6,b","res 6,c","res 6,d","res 6,e","res 6,h","res 6,l","res 6,(hl)","res 6,a","res 7,b","res 7,c","res 7,d","res 7,e","res 7,h","res 7,l","res 7,(hl)","res 7,a",
    "set 0,b","set 0,c","set 0,d","set 0,e","set 0,h","set 0,l","set 0,(hl)","set 0,a","set 1,b","set 1,c","set 1,d","set 1,e","set 1,h","set 1,l","set 1,(hl)","set 1,a",
    "set 2,b","set 2,c","set 2,d","set 2,e","set 2,h","set 2,l","set 2,(hl)","set 2,a","set 3,b","set 3,c","set 3,d","set 3,e","set 3,h","set 3,l","set 3,(hl)","set 3,a",
    "set 4,b","set 4,c","set 4,d","set 4,e","set 4,h","set 4,l","set 4,(hl)","set 4,a","set 5,b","set 5,c","set 5,d","set 5,e","set 5,h","set 5,l","set 5,(hl)","set 5,a",
    "set 6,b","set 6,c","set 6,d","set 6,e","set 6,h","set 6,l","set 6,(hl)","set 6,a","set 7,b","set 7,c","set 7,d","set 7,e","set 7,h","set 7,l","set 7,(hl)","set 7,a"};
