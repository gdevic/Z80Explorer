#ifndef CLASSCONTROLLER_H
#define CLASSCONTROLLER_H

#include "ClassChip.h"
#include "ClassColors.h"
#include "ClassScript.h"
#include "ClassSimZ80.h"
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
    inline ClassChip     &getChip()     { return m_chip; }   // Returns a reference to the chip class
    inline ClassColors   &getColors()   { return m_colors; } // Returns a reference to the colors class
    inline ClassScript   &getScript()   { return m_script; } // Returns a reference to the script class
    inline ClassSimZ80   &getSimZ80()   { return m_simz80; } // Returns a reference to the Z80 simulator class
    inline ClassWatch    &getWatch()    { return m_watch; }  // Returns a reference to the watch class
    inline ClassNetlist  &getNetlist()  { return m_simz80; } // Returns a reference to the netlist class (a subclass)
    inline ClassTrickbox &getTrickbox() { return m_trick; }  // Returns a reference to the Trickbox class

    inline uint8_t readMem(uint16_t ab)           // Reads from simulated RAM
        { return m_trick.readMem(ab); }
    inline void writeMem(uint16_t ab, uint8_t db) // Writes to simulated RAM
        { m_trick.writeMem(ab, db); }
    inline uint8_t readIO(uint16_t ab)            // Reads from simulated IO space
        { return m_trick.readIO(ab); }
    inline void writeIO(uint16_t ab, uint8_t db)  // Writes to simulated IO space
        { m_trick.writeIO(ab, db); }
    bool loadHex(QString fileName)                // Loads file into simulated RAM memory
        { return m_trick.loadHex(fileName); }
    void readState(z80state &state)               // Reads chip state structure
        { m_simz80.readState(state); }

    const QStringList getFormats(QString name); // Returns a list of formats applicable to the signal name (a net or a bus)
    enum FormatNet { Logic, TransUp, TransDown, TransAny };
    enum FormatBus { Hex, Bin, Oct, Dec, Ascii, Disasm };
    const QString formatBus(uint fmt, uint value, uint width); // Returns the formatted string for a bus type value

    // Simulator calls this on every half-clock cycle, controller will directly dispatch to modules that need it
    inline void onTick(uint ticks)          // Recieves the simulation half-cycle message and broadcast it to whomever needs it
        { m_trick.onTick(ticks); }
    // Requests operations on net names; this class will dispatch those operations via eventNetName() signal
    void setNetName(const QString name, const net_t); // Sets the name (alias) for a net
    void renameNet(const QString name, const net_t); // Renames a net using the new name
    void deleteNetName(const net_t);        // Deletes the current name of a specified net

public slots:
    uint doReset();                         //* Run the chip reset sequence, returns the number of clocks thet reset took
    void doRunsim(uint ticks);              //* Runs the simulation for the given number of clocks

signals:
    void shutdown();                        //* Application is shutting down, save your work!
    //                                      //* <- Methods of the scripting object "control"

signals:
    void onRunStarting(uint);               // Called by the sim when it is starting the simulation
    void onRunHeartbeat(uint);              // Called by the sim every 500ms when running
    void onRunStopped(uint);                // Called by the sim when the current run stops at a given half-cycle
    void eventNetName(Netop op, const QString name, const net_t); // Dispatches operations on net names

private:
    ClassChip     m_chip;       // Global chip resource class
    ClassColors   m_colors;     // Global application colors
    ClassScript   m_script;     // Global scripting support
    ClassSimZ80   m_simz80;     // Global Z80 simulator class
    ClassWatch    m_watch;      // Global watchlist
    ClassTrickbox m_trick;      // Global trickbox supporting environment
};

extern ClassController controller;

#endif // CLASSCONTROLLER_H
