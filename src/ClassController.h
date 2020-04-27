#ifndef CLASSCONTROLLER_H
#define CLASSCONTROLLER_H

#include "ClassChip.h"
#include "ClassColors.h"
#include "ClassScript.h"
#include "ClassSim.h"
#include "ClassSimX.h"
#include "ClassTrickbox.h"
#include "ClassWatch.h"

/*
 * Controller class implements the controller pattern, handling all requests
 */
class ClassController : public QObject
{
    Q_OBJECT
public:
    explicit ClassController() {};
    bool init(QScriptEngine *); // Initialize controller classes and variables

public: // API
    inline ClassChip    &getChip()    { return m_chip; }    // Returns a reference to the chip class
    inline ClassColors  &getColors()  { return m_colors; }  // Returns a reference to the colors class
    inline ClassScript  &getScript()  { return m_script; }  // Returns a reference to the script class
    inline ClassSim     &getSim()     { return m_sim; }     // Returns a reference to the sim class
    inline ClassSimX    &getSimx()    { return m_simx; }    // Returns a reference to the simx class
    inline ClassWatch   &getWatch()   { return m_watch; }   // Returns a reference to the watch class
    inline ClassNetlist &getNetlist() { return m_simx; }    // Returns a reference to the netlist class (a subclass)

    uint8_t readMem(uint16_t ab)            // Read environment RAM
        { return m_trick.readMem(ab); }
    void writeMem(uint16_t ab, uint8_t db)  // Write environment RAM
        { m_trick.writeMem(ab, db); }
    uint8_t readIO(uint16_t ab)             // Read environment IO
        { return m_trick.readIO(ab); }
    void writeIO(uint16_t ab, uint8_t db)   // Write environment IO
        { m_trick.writeIO(ab, db); }
    bool loadIntelHex(QString fileName)     // Loads file into RAM memory
        { return m_trick.loadIntelHex(fileName); }
    void readState(z80state &state)         // Reads chip state structure
        { m_simx.readState(state); }

    const QStringList getFormats(QString name); // Returns a list of formats applicable to the signal name (a net or a bus)
    enum FormatNet { Logic, TransUp, TransDown, TransAny };
    enum FormatBus { Hex, Bin, Oct, Dec, Ascii };
    const QString formatBus(uint fmt, uint value, uint width); // Returns the formatted string for a bus type value

    void setNetName(const QString name, net_t); // Sets a name (alias) for a net number

public slots:
    uint doReset();                         // Run the chip reset sequence, returns the number of clocks thet reset took
    void doRunsim(uint ticks);              // Runs the simulation for the given number of clocks

signals:
    void shutdown();                        // Application is shutting down, save your work!
    void echo(char e);                      // Echo a character onto the virtual console
    uint onRunStopped(uint);                // Called by the sim when the current run stops at a given half-cycle
    void netNameChanged();                  // Net name changed

private:
    ClassChip     m_chip;       // Global chip resource class
    ClassColors   m_colors;     // Global application colors
    ClassScript   m_script;     // Global scripting support
    ClassSimX     m_simx;       // Global simulator simx class
    ClassSim      m_sim;        // Global simulator sim class
    ClassWatch    m_watch;      // Global watchlist
    ClassTrickbox m_trick;      // Global trickbox supporting environment
};

extern ClassController controller;

#endif // CLASSCONTROLLER_H
