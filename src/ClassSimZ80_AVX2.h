#ifndef CLASSSIMZ80_AVX2_H
#define CLASSSIMZ80_AVX2_H

#include "AppTypes.h"
#include "z80state.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QHash>

// Cache line size for alignment
#define CACHE_LINE_SIZE 64

// AVX2-optimized Net structure using raw arrays instead of QVector
struct NetAVX2
{
    tran_t* gatesTrans;         // Array of transistor indices (gates this net controls)
    tran_t* c1c2sTrans;         // Array of transistor indices (transistors connected to this net)
    uint16_t gatesCount;        // Number of gates
    uint16_t c1c2sCount;        // Number of c1c2 connections
    bool state;                 // Current voltage state
    bool floats;                // Can float (hi-Z)
    bool isHigh;                // Being pulled high
    bool isLow;                 // Being pulled low
    bool hasPullup;             // Has permanent pull-up resistor
};

/*
 * ClassSimZ80_AVX2 implements an AVX2 optimized Z80 chip netlist simulator
 * Uses Structure-of-Arrays layout and raw pointer arrays for maximum performance
 */
class ClassSimZ80_AVX2 : public QObject
{
    Q_OBJECT

public:
    explicit ClassSimZ80_AVX2();
    ~ClassSimZ80_AVX2();

    bool loadResources(const QString dir);  // Load and convert netlist data
    bool initChip();                        // One-time chip initialization
    void readState(z80state &z);            // Reads chip state into a state structure
    uint doReset();                         // Run chip reset sequence
    void doRunsim(uint ticks);              // Run the simulation for the given number of clocks
    bool setPin(uint index, pin_t p);       // Sets an input pin to a value
    bool isRunning() { return m_runcount; }
    uint16_t getPC();
    uint getCurrentHCycle() { return m_hcycletotal; }
    uint getEstHz() { return m_estHz; }

    // Net name lookup (delegated interface)
    net_t get(const QString &name) { return m_netnums.contains(name) ? m_netnums[name] : 0; }
    const QString &get(net_t n) { return m_netnames[n]; }

    // Netlist query methods (compatible with ClassNetlist interface)
    uint getNetlistCount() { return MAX_NETS; }
    bool getNetState(net_t i) { return m_netlist[i].state; }
    bool isNetOrphan(net_t n) { return m_netlist[n].gatesCount == 0 && m_netlist[n].c1c2sCount == 0; }
    bool isNetPulledUp(net_t n) { return m_netlist[n].hasPullup; }
    bool isNetGateless(net_t n) { return m_netlist[n].gatesCount == 0; }

public slots:
    void onShutdown();

private slots:
    void onTimeout();

private:
    // Memory/IO handlers
    void handleMemRead(uint16_t ab);
    void handleMemWrite(uint16_t ab);
    void handleIORead(uint16_t ab);
    void handleIOWrite(uint16_t ab);
    void handleIrq();

    void setDB(uint8_t db);
    void set(bool on, const QString &name);
    __forceinline void set(bool on, net_t n);  // Fast version using cached net_t

    //==================== AVX2 OPTIMIZED SIMULATOR ====================

    void halfCycle();

    // AVX2 optimized bitset operations
    __forceinline void clearBitset_AVX2(uint64_t* bitset);
    __forceinline bool testBit(const uint64_t* bitset, net_t n);
    __forceinline void setBit(uint64_t* bitset, net_t n);

    // Core simulation functions
    __forceinline void recalcNetlist();
    __forceinline void recalcNet(net_t n);
    __forceinline bool getNetValue();
    __forceinline void getNetGroup(net_t n);
    __forceinline void addNetToGroup(net_t n);
    __forceinline void addRecalcNet(net_t n);

    // Bulk operations
    void allNets();

    // Read operations
    uint8_t readByte(const QString &name);
    pin_t readBit(const QString &name);
    pin_t readBit(net_t n);
    uint16_t readAB();
    pin_t getNetStateEx(net_t n);

    //==================== DATA STRUCTURES ====================

    // Structure-of-Arrays for transistors (cache-line aligned)
    alignas(CACHE_LINE_SIZE) uint8_t m_transOn[MAX_TRANS];      // ON state (0 or 1)
    alignas(CACHE_LINE_SIZE) net_t m_transC1[MAX_TRANS];        // c1 (source) net
    alignas(CACHE_LINE_SIZE) net_t m_transC2[MAX_TRANS];        // c2 (drain) net
    alignas(CACHE_LINE_SIZE) net_t m_transGate[MAX_TRANS];      // Gate net

    // AVX2-optimized netlist array
    NetAVX2 m_netlist[MAX_NETS];

    // Memory pools for net connection arrays (single allocation)
    tran_t* m_gatesPool;        // Pool for all gates arrays
    tran_t* m_c1c2sPool;        // Pool for all c1c2s arrays
    size_t m_gatesPoolSize;
    size_t m_c1c2sPoolSize;

    // Work lists (cache-line aligned)
    alignas(CACHE_LINE_SIZE) net_t m_list[MAX_NETS];
    alignas(CACHE_LINE_SIZE) net_t m_recalcList[MAX_NETS];
    alignas(CACHE_LINE_SIZE) net_t m_group[MAX_NETS];
    int m_listIndex;
    int m_recalcListIndex;
    int m_groupIndex;

    // Bitsets for O(1) duplicate detection (512 bytes each, 8 cache lines)
    // Size: (MAX_NETS + 63) / 64 = 57 uint64_t, round up to 64 for AVX-512 alignment
    alignas(CACHE_LINE_SIZE) uint64_t m_groupBitset[64];
    alignas(CACHE_LINE_SIZE) uint64_t m_recalcBitset[64];

    // Special net numbers (cached for performance - avoid QString lookups in hot path)
    net_t ngnd, npwr, nclk;
    net_t n_rfsh, n_m1, n_mreq, n_rd, n_wr, n_iorq, n_t2, n_t3;
    net_t n_db[8];   // db0-db7 cached for setDB performance
    net_t n_ab[16];  // ab0-ab15 cached for readAB performance

    // Net name lookup
    QString m_netnames[MAX_NETS];
    QHash<QString, net_t> m_netnums;
    QHash<QString, QVector<net_t>> m_buses;

    //==================== TIMER/STATE ====================

    QTimer m_timer;
    uint m_estHz {};
    QElapsedTimer m_elapsed;
    QAtomicInt m_runcount {};
    QAtomicInt m_hcyclecnt {};
    QAtomicInt m_hcycletotal {};

    //==================== RESOURCE LOADING ====================

    bool loadNetNames(const QString fileName, bool loadCustom);
    bool loadTransdefs(const QString dir);
    bool loadPullups(const QString dir);
    void convertToAVX2Layout();
};

#endif // CLASSSIMZ80_AVX2_H
