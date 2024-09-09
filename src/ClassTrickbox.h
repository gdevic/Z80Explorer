#ifndef CLASSTRICKBOX_H
#define CLASSTRICKBOX_H

#include <QObject>

// Defines the layout of the trickbox control area
#pragma pack(push,2)
#define MAX_PIN_CTRL 5
struct trick
{
    uint16_t stop;              // Writing to this address immediately stops the simulation
    uint16_t cycleStop;         // Cycle number at which to stop the simulation
    uint32_t curCycle;          // Current cycle number (low 16 bits + high 16 bits)
    struct
    {
        uint16_t atCycle;       // Cycle number at which to assert a pin
        uint16_t atPC;          // PC address at which to assert a pin
        uint16_t hold;          // Number of cycles to hold it asserted
    } pinCtrl[MAX_PIN_CTRL];    // That, for 5 pins: INT, NMI, BUSRQ, WAIT, RESET
};
#pragma pack(pop)

/*
 * This class implements the supporting environment for the simulation
 */
class ClassTrickbox : public QObject
{
    Q_OBJECT                                //* <- Methods of the scripting object "monitor" below
public:
    explicit ClassTrickbox(QObject *parent = nullptr);

    void reset();                           // Reset the control counters etc.
    void onTick(uint ticks);                // Called by the simulator on every half-clock tick
    Q_PROPERTY(bool enabled MEMBER m_trickEnabled) //* Enables or disables trickbox control
    Q_PROPERTY(uint rom MEMBER m_rom);      //* Designates the initial memory block as read-only

public slots:
    bool loadHex(const QString fileName);   //* Loads a HEX file into simulated RAM; empty name for last loaded
    bool loadBin(const QString fileName, quint16 address); //* Loads a binary file to simulated RAM at the given address
    bool saveBin(const QString fileName, quint16 address, uint size); //* Saves the content of the simulated RAM
    void zx();                              //* A little something
    quint8 readMem(quint16 ab);             //* Reads from simulated RAM
    void writeMem(quint16 ab, quint8 db);   //* Writes to simulated RAM
    quint8 readIO(quint16 ab);              //* Reads from simulated IO space
    void writeIO(quint16 ab, quint8 db);    //* Writes to simulated IO space
    const QString readState();              //* Reads sim monitor state
    void stopAt(quint16 hcycle)             //* Stops execution at the given hcycle
        { m_trick->cycleStop = hcycle; emit refresh(); }
    void breakWhen(quint16 net, quint8 value); //* Stops running when the given net number's state equals the value
    void set(QString pin, quint8 value);    //* Sets named pin to a value (0,1,2)
    void setAt(QString pin, quint16 hcycle, quint16 hold); //* Activates (sets to 0) a named pin at the specified hcycle
    void setAtPC(QString pin, quint16 addr, quint16 hold); //* Activates (sets to 0) a named pin when PC equals the address

signals:
    void echo(char c);                      //* Request to write out a character to a terminal
    void echo(QString s);                   //* Request to write out a string to a terminal
    void refresh();                         // Outgoing request to refresh the monitor information display

private:
    bool readHex(QString fileName);
    uint8_t m_mem[65536] {};                // Simulated 64K memory
    uint8_t m_mio[65536];                   // Simulated 64K IO space as r/w
    trick *m_trick;                         // Start of the trickbox control area
    bool m_trickWriteEven {true};           // Even/odd write address to the control area
    bool m_trickEnabled {true};             // Trickbox control is enabled
    uint m_rom {0};                         // Designates the initial memory block as read-only
    QString m_lastLoadedHex;                // File name of the last loaded hex code
    quint16 m_bpnet {};                     // Net number to check for break
    quint8 m_bpval {};                      // Value to break at
};

#endif // CLASSTRICKBOX_H
