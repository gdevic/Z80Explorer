#ifndef CLASSTRICKBOX_H
#define CLASSTRICKBOX_H

#include <QObject>

// Defines the layout of the trickbox memory arena
#pragma pack(push,2)
struct trick
{
    uint16_t cycleStop; // Cycle number at which to stop the simulation
    struct
    {
        uint16_t cycle; // Cycle number at which to assert a pin
        uint16_t count; // Number of cycles to hold it asserted
    } pinCtrl[5];       // That, for 5 pins: INT, NMI, BUSRQ, WAIT, RESET
    uint32_t curCycle;  // Current cycle number (low 16 bits + high 16 bits)
};
#pragma pack(pop)

/*
 * This class implements the supporting environment for the simulation
 */
class ClassTrickbox : public QObject
{
    Q_OBJECT
public:
    explicit ClassTrickbox(QObject *parent = nullptr);

    bool loadIntelHex(const QString fileName); // Loads RAM memory with the content of an Intel HEX file
    void reset();                           // Reset the control counters etc.
    void onTick(uint ticks);                // Called by the simulator on every half-clock tick

    uint8_t readMem(uint16_t ab);           // Reads from simulated RAM
    void writeMem(uint16_t ab, uint8_t db); // Writes to simulated RAM
    uint8_t readIO(uint16_t ab);            // Reads from simulated IO space
    void writeIO(uint16_t ab, uint8_t db);  // Writes to simulated IO space

signals:
    void echo(char c);                      // Character to a terminal

private:
    bool readHex(QString fileName);
    uint8_t m_mem[65536] {};                // Simulated 64K memory
    trick *m_trick;                         // Start of the trickbox memory arena
    bool m_enableTrick {};                  // Enable trickbox's sim flow control
};

#endif // CLASSTRICKBOX_H
