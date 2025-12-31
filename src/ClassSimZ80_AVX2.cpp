#include "ClassSimZ80_AVX2.h"
#include "ClassController.h"
#include <QFile>
#include <QStringBuilder>
#include <QtConcurrent>

ClassSimZ80_AVX2::ClassSimZ80_AVX2()
    : m_gatesPool(nullptr)
    , m_c1c2sPool(nullptr)
    , m_gatesPoolSize(0)
    , m_c1c2sPoolSize(0)
    , m_listIndex(0)
    , m_recalcListIndex(0)
    , m_groupIndex(0)
    , ngnd(0)
    , npwr(0)
    , nclk(0)
{
    connect(&m_timer, &QTimer::timeout, this, &ClassSimZ80_AVX2::onTimeout);

    // Zero-initialize all arrays
    memset(m_transOn, 0, sizeof(m_transOn));
    memset(m_transC1, 0, sizeof(m_transC1));
    memset(m_transC2, 0, sizeof(m_transC2));
    memset(m_transGate, 0, sizeof(m_transGate));
    memset(m_netlist, 0, sizeof(m_netlist));
    memset(m_list, 0, sizeof(m_list));
    memset(m_recalcList, 0, sizeof(m_recalcList));
    memset(m_group, 0, sizeof(m_group));
    memset(m_groupBitset, 0, sizeof(m_groupBitset));
    memset(m_recalcBitset, 0, sizeof(m_recalcBitset));
}

ClassSimZ80_AVX2::~ClassSimZ80_AVX2()
{
    // Free memory pools
    if (m_gatesPool)
        _aligned_free(m_gatesPool);
    if (m_c1c2sPool)
        _aligned_free(m_c1c2sPool);
}

void ClassSimZ80_AVX2::onShutdown()
{
    doRunsim(0); // Stop simulation
}

//=============================================================================
// AVX2 OPTIMIZED BITSET OPERATIONS
//=============================================================================

// Clear 512 bytes (64 uint64_t) using AVX2
// 16 stores of 32 bytes each
__forceinline void ClassSimZ80_AVX2::clearBitset_AVX2(uint64_t* bitset)
{
    __m256i zero = _mm256_setzero_si256();
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 0), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 4), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 8), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 12), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 16), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 20), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 24), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 28), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 32), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 36), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 40), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 44), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 48), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 52), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 56), zero);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bitset + 60), zero);
}

// Test bit using x64 BT instruction
__forceinline bool ClassSimZ80_AVX2::testBit(const uint64_t* bitset, net_t n)
{
    return _bittest64(reinterpret_cast<const __int64*>(bitset), n);
}

// Set bit using x64 BTS instruction
__forceinline void ClassSimZ80_AVX2::setBit(uint64_t* bitset, net_t n)
{
    _bittestandset64(reinterpret_cast<__int64*>(bitset), n);
}

//=============================================================================
// RESOURCE LOADING
//=============================================================================

bool ClassSimZ80_AVX2::loadResources(const QString dir)
{
    qInfo() << "Loading AVX2-optimized netlist resources from" << dir;

    if (loadNetNames(dir + "/nodenames.js", false))
    {
        loadNetNames(dir + "/netnames.js", true);

        ngnd = get("vss");
        npwr = get("vcc");
        nclk = get("clk");

        qInfo() << "Checking that vss,vcc,clk nets are numbered 1,2,3";
        if (ngnd == 1 && npwr == 2 && nclk == 3)
        {
            if (loadTransdefs(dir) && loadPullups(dir))
            {
                convertToAVX2Layout();
                qInfo() << "Completed loading AVX2-optimized netlist resources";
                return true;
            }
        }
        else
        {
            qCritical() << "Expected vss=1, vcc=2, clk=3 but got" << ngnd << npwr << nclk;
        }
    }
    qCritical() << "Loading AVX2-optimized netlist resource failed";
    return false;
}

bool ClassSimZ80_AVX2::loadNetNames(const QString fileName, bool loadCustom)
{
    qInfo() << "Loading" << fileName;
    QFile file(fileName);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        while (!in.atEnd())
        {
            line = in.readLine();
            int comment = line.indexOf('/');
            if (comment != -1)
                line = line.left(comment).trimmed();
            if (line.indexOf(':') != -1)
            {
                line.chop(1);
                list = line.split(QLatin1Char(':'), Qt::SkipEmptyParts);
                if (list.length() == 2)
                {
                    QString name = list[0].trimmed();
                    net_t n = list[1].toUInt();

                    if (loadCustom)
                    {
                        QStringList buslist = list[1].replace('[', ' ').replace(']', ' ').split(QLatin1Char(','), Qt::SkipEmptyParts);
                        if (buslist.count() > 1)
                        {
                            QVector<net_t> nets;
                            for (const auto &n : buslist)
                                nets.append(n.toUInt());
                            m_buses[name] = nets;
                        }
                        else
                        {
                            if (m_netnums.contains(name))
                            {
                                net_t old = m_netnums[name];
                                m_netnames[old] = QString();
                                m_netnums.remove(name);
                            }
                            m_netnames[n] = name;
                            m_netnums[name] = n;
                        }
                    }
                    else
                    {
                        if (!m_netnums.contains(name) && m_netnames[n].isEmpty())
                        {
                            m_netnames[n] = name;
                            m_netnums[name] = n;
                        }
                    }
                }
            }
        }
        return true;
    }
    qCritical() << "Error opening" << fileName;
    return false;
}

bool ClassSimZ80_AVX2::loadTransdefs(const QString dir)
{
    QString transdefs_file = dir + "/transdefs.js";
    qInfo() << "Loading" << transdefs_file;
    QFile file(transdefs_file);

    // Temporary storage for building connection lists
    QVector<tran_t> tempGates[MAX_NETS];
    QVector<tran_t> tempC1c2s[MAX_NETS];

    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        uint count = 0, pull_ups = 0;

        while (!in.atEnd())
        {
            line = in.readLine();
            if (line.startsWith('['))
            {
                line.replace('[', ' ').replace(']', ' ');
                line.chop(2);
                list = line.split(QLatin1Char(','), Qt::SkipEmptyParts);
                if ((list.length() == 14) && (list[0].length() > 2))
                {
                    if (list[13] != "true")
                    {
                        QString tnum = list[0].mid(3, list[0].length() - 4);
                        tran_t i = tnum.toUInt();
                        Q_ASSERT(i < MAX_TRANS);

                        // Store in SoA format
                        m_transGate[i] = list[1].toUInt();
                        m_transC1[i] = list[2].toUInt();
                        m_transC2[i] = list[3].toUInt();
                        m_transOn[i] = 0; // Off by default

                        // Normalize: c1 should be the non-power net
                        if (m_transC1[i] <= nclk)
                            std::swap(m_transC1[i], m_transC2[i]);

                        // Build connection lists
                        tempGates[m_transGate[i]].append(i);
                        tempC1c2s[m_transC1[i]].append(i);
                        tempC1c2s[m_transC2[i]].append(i);

                        count++;
                    }
                    else
                        pull_ups++;
                }
            }
        }

        if (pull_ups != 32)
        {
            qCritical() << "Unexpected number of pull-ups in transdefs.js";
            return false;
        }

        // Calculate total pool sizes needed
        m_gatesPoolSize = 0;
        m_c1c2sPoolSize = 0;
        for (int n = 0; n < MAX_NETS; n++)
        {
            m_gatesPoolSize += tempGates[n].size();
            m_c1c2sPoolSize += tempC1c2s[n].size();
        }

        // Allocate memory pools (aligned for potential SIMD access)
        m_gatesPool = static_cast<tran_t*>(_aligned_malloc(m_gatesPoolSize * sizeof(tran_t), CACHE_LINE_SIZE));
        m_c1c2sPool = static_cast<tran_t*>(_aligned_malloc(m_c1c2sPoolSize * sizeof(tran_t), CACHE_LINE_SIZE));

        if (!m_gatesPool || !m_c1c2sPool)
        {
            qCritical() << "Failed to allocate memory pools";
            return false;
        }

        // Copy data to pools and set up pointers
        tran_t* gatesPtr = m_gatesPool;
        tran_t* c1c2sPtr = m_c1c2sPool;

        for (int n = 0; n < MAX_NETS; n++)
        {
            // Gates
            m_netlist[n].gatesCount = uint16_t(tempGates[n].size());
            if (m_netlist[n].gatesCount > 0)
            {
                m_netlist[n].gatesTrans = gatesPtr;
                memcpy(gatesPtr, tempGates[n].constData(), tempGates[n].size() * sizeof(tran_t));
                gatesPtr += tempGates[n].size();
            }
            else
            {
                m_netlist[n].gatesTrans = nullptr;
            }

            // C1C2s
            m_netlist[n].c1c2sCount = uint16_t(tempC1c2s[n].size());
            if (m_netlist[n].c1c2sCount > 0)
            {
                m_netlist[n].c1c2sTrans = c1c2sPtr;
                memcpy(c1c2sPtr, tempC1c2s[n].constData(), tempC1c2s[n].size() * sizeof(tran_t));
                c1c2sPtr += tempC1c2s[n].size();
            }
            else
            {
                m_netlist[n].c1c2sTrans = nullptr;
            }
        }

        qInfo() << "Loaded" << count << "transistors into AVX2-optimized SoA layout";
        qInfo() << "Gates pool:" << m_gatesPoolSize << "entries, C1C2s pool:" << m_c1c2sPoolSize << "entries";

        return true;
    }
    qCritical() << "Error opening transdefs.js";
    return false;
}

bool ClassSimZ80_AVX2::loadPullups(const QString dir)
{
    QString segdefs_file = dir + "/segdefs.js";
    qInfo() << "Loading" << segdefs_file;
    QFile file(segdefs_file);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        uint count = 0;

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
                    m_netlist[i].hasPullup = list[1].contains('+');
                    m_netlist[i].isHigh = list[1].contains('+');
                    if (m_netlist[i].hasPullup)
                        count++;
                }
            }
        }
        qInfo() << "Number of pullups:" << count;
        return true;
    }
    qCritical() << "Error opening segdefs.js";
    return false;
}

void ClassSimZ80_AVX2::convertToAVX2Layout()
{
    // Any additional conversion needed after loading
    qInfo() << "AVX2-optimized data layout conversion complete";
}

//=============================================================================
// CHIP INITIALIZATION
//=============================================================================

bool ClassSimZ80_AVX2::initChip()
{
    Q_ASSERT(ngnd == 1);
    Q_ASSERT(npwr == 2);

    // Initialize GND and Vcc
    m_netlist[ngnd].state = false;
    m_netlist[npwr].state = true;

    // Initialize nets which can float
    net_t mreq = get("_mreq");
    net_t iorq = get("_iorq");
    net_t rd = get("_rd");
    net_t wr = get("_wr");
    net_t ab0 = get("ab0");
    net_t db0 = get("db0");

    if (mreq && iorq && rd && wr && ab0 && db0)
    {
        m_netlist[mreq].floats = true;
        m_netlist[iorq].floats = true;
        m_netlist[rd].floats = true;
        m_netlist[wr].floats = true;

        m_netlist[get("dbus0")].floats = true;
        m_netlist[get("ubus0")].floats = true;
        m_netlist[get("vbus0")].floats = true;

        for (int i = 0; i < 16; i++)
            m_netlist[get(QString("ab%1").arg(i))].floats = true;
    }
    else
    {
        qCritical() << "Unknown net name";
        return false;
    }

    // Turn off all transistors
    memset(m_transOn, 0, sizeof(m_transOn));

    return true;
}

//=============================================================================
// SIMULATION CONTROL
//=============================================================================

void ClassSimZ80_AVX2::onTimeout()
{
    m_estHz = uint(uint(m_hcyclecnt) / 2.0) / (m_elapsed.elapsed() / 1000.0);
    if (m_runcount <= 0)
        m_timer.stop();
    else
        emit ::controller.onRunHeartbeat(m_hcyclecnt);
}

bool ClassSimZ80_AVX2::setPin(uint index, pin_t p)
{
    const static QStringList pins = { "_int", "_nmi", "_busrq", "_wait", "_reset" };
    if (index < uint(pins.count()))
    {
        set(p, pins[index]);
        return true;
    }
    return false;
}

void ClassSimZ80_AVX2::doRunsim(uint ticks)
{
    if (!m_runcount && !ticks)
        return;
    if (m_runcount)
        m_runcount = ticks;
    else
    {
        emit ::controller.onRunStarting(ticks);
        if (ticks < 3)
        {
            while (ticks--)
                halfCycle();
            emit ::controller.onRunStopped(m_hcycletotal);
        }
        else
        {
            m_runcount = ticks;
            m_timer.start(500);
            m_elapsed.start();
            m_hcyclecnt = 0;
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

uint ClassSimZ80_AVX2::doReset()
{
    if (m_runcount)
    {
        m_runcount = 0;
        return 0;
    }

    set(0, "_reset");
    set(1, "clk");
    set(1, "_busrq");
    set(1, "_int");
    set(1, "_nmi");
    set(1, "_wait");

    allNets();
    recalcNetlist();

    m_hcycletotal = 0;

    for (int i = 0; i < 8; i++)
        halfCycle();

    set(1, "_reset");

    return m_hcycletotal;
}

//=============================================================================
// CORE SIMULATION - AVX2 OPTIMIZED HOT PATH
//=============================================================================

__forceinline void ClassSimZ80_AVX2::halfCycle()
{
    const pin_t clk = readBit("clk");
    if (!clk && readBit("_rfsh"))
    {
        const bool m1   = readBit("_m1");
        const bool rfsh = 1;
        const bool mreq = readBit("_mreq");
        const bool rd   = readBit("_rd");
        const bool wr   = readBit("_wr");
        const bool iorq = readBit("_iorq");
        const bool t2   = readBit("t2");
        const bool t3   = readBit("t3");

        if (!m1 && rfsh && !mreq && !rd &&  wr &&  iorq && t2)
            handleMemRead(readAB());
        else if ( m1 && rfsh && !mreq && !rd &&  wr &&  iorq && t3)
            handleMemRead(readAB());
        else if ( m1 && rfsh && !mreq &&  rd && !wr &&  iorq && t3)
            handleMemWrite(readAB());
        else if ( m1 && rfsh &&  mreq && !rd &&  wr && !iorq && t3)
            handleIORead(readAB());
        else if ( m1 && rfsh &&  mreq &&  rd && !wr && !iorq && t3)
            handleIOWrite(readAB());
        else if (!m1 && rfsh &&  mreq &&  rd &&  wr && !iorq)
            handleIrq();
    }

    set(!clk, "clk");

    if (::controller.getWatch().getWatchlistLen())
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

    ::controller.onTick(m_hcycletotal);

    m_hcyclecnt.fetchAndAddRelaxed(1);
    m_hcycletotal.fetchAndAddRelaxed(1);
}

__forceinline void ClassSimZ80_AVX2::recalcNetlist()
{
    m_recalcListIndex = 0;
    clearBitset_AVX2(m_recalcBitset);

    while (m_listIndex)
    {
        for (int i = 0; i < m_listIndex; i++)
            recalcNet(m_list[i]);

        memcpy(m_list, m_recalcList, m_recalcListIndex * sizeof(net_t));
        m_listIndex = m_recalcListIndex;
        m_recalcListIndex = 0;
        clearBitset_AVX2(m_recalcBitset);
    }
}

__forceinline void ClassSimZ80_AVX2::recalcNet(net_t n)
{
    if (n <= npwr) return;

    getNetGroup(n);
    bool newState = getNetValue();

    // Process all nets in the group
    net_t* groupEnd = m_group + m_groupIndex;
    for (net_t* p = m_group; p < groupEnd; p++)
    {
        NetAVX2& net = m_netlist[*p];
        if (net.state == newState) continue;
        net.state = newState;

        // Get the transistor indices for this net's gates
        tran_t* gates = net.gatesTrans;
        uint16_t gatesCount = net.gatesCount;

        if (newState)
        {
            // Net went HIGH - turn on transistors
            for (uint16_t i = 0; i < gatesCount; i++)
            {
                tran_t t = gates[i];
                if (!m_transOn[t])
                {
                    m_transOn[t] = 1;
                    addRecalcNet(m_transC1[t]);
                }
            }
        }
        else
        {
            // Net went LOW - turn off transistors
            for (uint16_t i = 0; i < gatesCount; i++)
            {
                tran_t t = gates[i];
                if (m_transOn[t])
                {
                    m_transOn[t] = 0;
                    addRecalcNet(m_transC1[t]);
                    addRecalcNet(m_transC2[t]);
                }
            }
        }
    }
}

__forceinline bool ClassSimZ80_AVX2::getNetValue()
{
    // Fast path: check first element for power connections
    if (m_group[0] <= npwr)
        return m_group[0] == npwr;

    // Check for pullup/pulldown and resolve floating nodes
    bool floatingHigh = false;
    net_t* groupEnd = m_group + m_groupIndex;

    for (net_t* p = m_group; p < groupEnd; p++)
    {
        NetAVX2& net = m_netlist[*p];
        if (net.isHigh) return true;
        if (net.isLow) return false;
        if (net.state) floatingHigh = true;
    }

    return floatingHigh;
}

__forceinline void ClassSimZ80_AVX2::getNetGroup(net_t n)
{
    m_groupIndex = 0;
    clearBitset_AVX2(m_groupBitset);
    addNetToGroup(n);
}

// CRITICAL HOT FUNCTION - This is 45% of CPU time
__forceinline void ClassSimZ80_AVX2::addNetToGroup(net_t n)
{
    // O(1) duplicate check using x64 BT instruction
    if (testBit(m_groupBitset, n))
        return;

    // Mark as visited using x64 BTS instruction
    setBit(m_groupBitset, n);

    // Power nets go at position 0 for fast detection
    if (n <= npwr)
    {
        m_group[m_groupIndex] = n;
        std::swap(m_group[0], m_group[m_groupIndex]);
        m_groupIndex++;
        return;
    }

    m_group[m_groupIndex++] = n;

    // Get connection array directly (no QVector overhead!)
    const tran_t* c1c2s = m_netlist[n].c1c2sTrans;
    const uint16_t count = m_netlist[n].c1c2sCount;

    // Unrolled inner loop for better ILP
    uint16_t i = 0;

    // Process 4 at a time when possible
    for (; i + 3 < count; i += 4)
    {
        tran_t t0 = c1c2s[i];
        tran_t t1 = c1c2s[i + 1];
        tran_t t2 = c1c2s[i + 2];
        tran_t t3 = c1c2s[i + 3];

        // Prefetch ahead
        if (i + 7 < count)
        {
            _mm_prefetch(reinterpret_cast<const char*>(&m_transOn[c1c2s[i + 4]]), _MM_HINT_T0);
        }

        // Process transistor 0
        if (m_transOn[t0])
        {
            net_t c1 = m_transC1[t0];
            net_t c2 = m_transC2[t0];
            net_t other = (c1 == n) ? c2 : ((c2 == n) ? c1 : 0);
            if (other) addNetToGroup(other);
        }

        // Process transistor 1
        if (m_transOn[t1])
        {
            net_t c1 = m_transC1[t1];
            net_t c2 = m_transC2[t1];
            net_t other = (c1 == n) ? c2 : ((c2 == n) ? c1 : 0);
            if (other) addNetToGroup(other);
        }

        // Process transistor 2
        if (m_transOn[t2])
        {
            net_t c1 = m_transC1[t2];
            net_t c2 = m_transC2[t2];
            net_t other = (c1 == n) ? c2 : ((c2 == n) ? c1 : 0);
            if (other) addNetToGroup(other);
        }

        // Process transistor 3
        if (m_transOn[t3])
        {
            net_t c1 = m_transC1[t3];
            net_t c2 = m_transC2[t3];
            net_t other = (c1 == n) ? c2 : ((c2 == n) ? c1 : 0);
            if (other) addNetToGroup(other);
        }
    }

    // Handle remaining elements
    for (; i < count; i++)
    {
        tran_t t = c1c2s[i];
        if (!m_transOn[t]) continue;

        net_t c1 = m_transC1[t];
        net_t c2 = m_transC2[t];
        net_t other = (c1 == n) ? c2 : ((c2 == n) ? c1 : 0);
        if (other)
            addNetToGroup(other);
    }
}

__forceinline void ClassSimZ80_AVX2::addRecalcNet(net_t n)
{
    if (n <= npwr) return;

    // O(1) duplicate check
    if (testBit(m_recalcBitset, n))
        return;

    setBit(m_recalcBitset, n);
    m_recalcList[m_recalcListIndex++] = n;
}

void ClassSimZ80_AVX2::allNets()
{
    m_listIndex = 0;
    for (net_t n = 0; n < MAX_NETS; n++)
    {
        if (n == ngnd || n == npwr)
            continue;
        if (m_netlist[n].gatesCount == 0 && m_netlist[n].c1c2sCount == 0)
            continue;
        m_list[m_listIndex++] = n;
    }
}

//=============================================================================
// DATA BUS AND PIN OPERATIONS
//=============================================================================

__forceinline void ClassSimZ80_AVX2::set(bool on, const QString &name)
{
    net_t n = get(name);
    if (m_netlist[n].isHigh == on)
        return;
    m_netlist[n].isHigh = on;
    m_netlist[n].isLow = !on;

    m_list[0] = n;
    m_listIndex = 1;
    recalcNetlist();
}

void ClassSimZ80_AVX2::setDB(uint8_t db)
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

void ClassSimZ80_AVX2::handleMemRead(uint16_t ab)
{
    uint8_t db = ::controller.readMem(ab);
    setDB(db);
}

void ClassSimZ80_AVX2::handleMemWrite(uint16_t ab)
{
    uint8_t db = readByte("db");
    ::controller.writeMem(ab, db);
}

void ClassSimZ80_AVX2::handleIORead(uint16_t ab)
{
    uint8_t db = ::controller.readIO(ab);
    setDB(db);
}

void ClassSimZ80_AVX2::handleIOWrite(uint16_t ab)
{
    uint8_t db = readByte("db");
    ::controller.writeIO(ab, db);
}

void ClassSimZ80_AVX2::handleIrq()
{
    uint8_t db = ::controller.readIO(0x81);
    setDB(db);
}

//=============================================================================
// READ OPERATIONS
//=============================================================================

uint16_t ClassSimZ80_AVX2::readAB()
{
    uint16_t value = 0;
    for (int i = 15; i >= 0; --i)
    {
        value <<= 1;
        value |= !!readBit(QString("ab" % QString::number(i)));
    }
    return value;
}

uint8_t ClassSimZ80_AVX2::readByte(const QString &name)
{
    uint value = 0;
    for (int i = 7; i >= 0; --i)
    {
        value <<= 1;
        value |= !!readBit(QString(name % QString::number(i)));
    }
    return value;
}

pin_t ClassSimZ80_AVX2::readBit(const QString &name)
{
    net_t n = get(name);
    Q_ASSERT(n < MAX_NETS);
    if (m_netlist[n].floats)
        return getNetStateEx(n);
    return m_netlist[n].state;
}

pin_t ClassSimZ80_AVX2::readBit(net_t n)
{
    Q_ASSERT(n < MAX_NETS);
    if (m_netlist[n].floats)
        return getNetStateEx(n);
    return m_netlist[n].state;
}

pin_t ClassSimZ80_AVX2::getNetStateEx(net_t n)
{
    tran_t* c1c2s = m_netlist[n].c1c2sTrans;
    uint16_t count = m_netlist[n].c1c2sCount;

    for (uint16_t i = 0; i < count; i++)
        if (m_transOn[c1c2s[i]])
            return !!m_netlist[n].state;

    if (m_netlist[n].hasPullup)
        return 1;
    return 2;
}

uint16_t ClassSimZ80_AVX2::getPC()
{
    return (readByte("reg_pch") << 8) | readByte("reg_pcl");
}

//=============================================================================
// STATE READ
//=============================================================================

void ClassSimZ80_AVX2::readState(z80state &z)
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
    z.wait = readBit("_wait");

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
    z.nED = readBit(265);
    z.nCB = readBit(263);
}
