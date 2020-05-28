#ifndef CLASSTRICKBOX_H
#define CLASSTRICKBOX_H

#include <QObject>

// Defines the layout of the trickbox memory arena
#pragma pack(push,2)
#define MAX_PIN_CTRL 5
struct trick
{
    uint16_t cycleStop;         // Cycle number at which to stop the simulation
    struct
    {
        uint16_t cycle;         // Cycle number at which to assert a pin
        uint16_t count;         // Number of cycles to hold it asserted
    } pinCtrl[MAX_PIN_CTRL];    // That, for 5 pins: INT, NMI, BUSRQ, WAIT, RESET
    uint32_t curCycle;          // Current cycle number (low 16 bits + high 16 bits)
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

    void reset();                           // Reset the control counters etc.
    void onTick(uint ticks);                // Called by the simulator on every half-clock tick

public slots:
    bool loadHex(const QString fileName);   //* Loads a HEX file into simulated RAM; empty name for last loaded
    quint8 readMem(quint16 ab);             //* Reads from simulated RAM
    void writeMem(quint16 ab, quint8 db);   //* Writes to simulated RAM
    quint8 readIO(quint16 ab);              //* Reads from simulated IO space
    void writeIO(quint16 ab, quint8 db);    //* Writes to simulated IO space
    const QString readState();              //* Reads sim monitor state
    void stopAt(quint16 hcycle)             //* Stops execution at the given hcycle
        { m_trick->cycleStop = hcycle; emit refresh(); }
    void set(QString pin, quint8 value = 0);//* Sets named pin to a value (0,1,2)
    void setAt(QString pin, quint16 hcycle, quint16 count = 6); //* Activates (sets to 0) named pin at the specified hcycle

signals:
    void echo(char c);                      //* Request to write out a character to a terminal
    void echo(QString s);                   //* Request to write out a string to a terminal
    void refresh();                         // Outgoing request to refresh the monitor information display

private:
    bool readHex(QString fileName);
    uint8_t m_mem[65536] {};                // Simulated 64K memory
    trick *m_trick;                         // Start of the trickbox memory arena
    bool m_enableTrick {true};              // Enable trickbox's sim flow control
    QString m_lastLoadedHex;                // File name of the last loaded hex code
};
//                                          //* <- Methods of the scripting object "monitor"

#endif // CLASSTRICKBOX_H
