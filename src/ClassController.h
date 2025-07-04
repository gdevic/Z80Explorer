#ifndef CLASSCONTROLLER_H
#define CLASSCONTROLLER_H

#include "ClassAnnotate.h"
#include "ClassVisual.h"
#include "ClassColors.h"
#include "ClassScript.h"
#include "ClassServer.h"
#include "ClassSimZ80.h"
#include "ClassTip.h"
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
    bool init(QJSEngine *);                     // Initialize controller classes and variables

public: // API
    inline ClassAnnotate &getAnnotation() { return m_annotate; }  // Returns a reference to the annotations class
    inline ClassVisual   &getChip()       { return m_chip; }      // Returns a reference to the chip class
    inline ClassColors   &getColors()     { return m_colors; }    // Returns a reference to the colors class
    inline ClassScript   &getScript()     { return m_script; }    // Returns a reference to the script class
    inline ClassServer   &getServer()     { return m_server; }    // Returns a reference to the server class
    inline ClassSimZ80   &getSimZ80()     { return m_simz80; }    // Returns a reference to the Z80 simulator class
    inline ClassWatch    &getWatch()      { return m_watch; }     // Returns a reference to the watch class
    inline ClassNetlist  &getNetlist()    { return m_simz80; }    // Returns a reference to the netlist class (a subclass)
    inline ClassTip      &getTip()        { return m_tips; }      // Returns a reference to the tips class
    inline ClassTrickbox &getTrickbox()   { return m_trick; }     // Returns a reference to the Trickbox class

    inline uint8_t readMem(uint16_t ab)           // Reads from simulated RAM
        { return m_trick.readMem(ab); }
    inline void writeMem(uint16_t ab, uint8_t db) // Writes to simulated RAM
        { m_trick.writeMem(ab, db); }
    inline uint8_t readIO(uint16_t ab)            // Reads from simulated IO space
        { return m_trick.readIO(ab); }
    inline void writeIO(uint16_t ab, uint8_t db)  // Writes to simulated IO space
        { m_trick.writeIO(ab, db); }
    bool loadHex(QString fileName)                // Loads file into simulated RAM memory; empty name for last loaded
        { return m_trick.loadHex(fileName); }
    bool patchHex(QString fileName)               // Merges file into simulated RAM memory; empty name for last loaded
        { return m_trick.patchHex(fileName); }
    void readState(z80state &state)               // Reads chip state structure
        { m_simz80.readState(state); }
    bool isSimRunning()                           // Returns true is the simulation is currently running
        { return m_simz80.isRunning(); }

    const QStringList getFormats(QString name); // Returns a list of formats applicable to the signal name (a net or a bus)
    enum FormatNet { Logic, Logic0Filled, Logic1Filled, TransUp, TransDown, TransAny };
    enum FormatBus { Hex, Bin, Oct, Dec, Ascii, Disasm, OnesComplement };
    const QString formatBus(uint fmt, uint value, uint width, bool decorated = true); // Returns the formatted string for a bus type value

    // Simulator calls this on every half-clock cycle, controller will directly dispatch to modules that need it
    inline void onTick(uint ticks)          // Recieves the simulation half-cycle message and broadcast it to whomever needs it
        { m_trick.onTick(ticks); }
    // Requests operations on net names; this class will dispatch those operations via eventNetName() signal
    void setNetName(const QString name, const net_t); // Sets the name (alias) for a net
    void renameNet(const QString name, const net_t); // Renames a net using the new name
    void deleteNetName(const net_t);        // Deletes the current name of a specified net

    // Misc utility functions
    bool compressFile(const QString &inFileName, const QString &outFileName); // Compresses a file, writing the result into another file
    bool uncompressFile(const QString &inFileName, const QString &outFileName); // Uncompresses a file, writing the result into another file

public slots:
    uint doReset();                         // Runs the chip reset sequence, returns the number of clocks thet reset took
    void doRunsim(uint ticks);              // Runs the simulation for the given number of clocks
    void save() { emit shutdown(); }        // Saves all modified files

signals:
    void onRunStarting(uint);               // Called by the sim when it is starting the simulation
    void onRunHeartbeat(uint);              // Called by the sim every 500ms when running
    void onRunStopped(uint);                // Called by the sim when the current run stops at a given half-cycle
    void eventNetName(Netop op, const QString name, const net_t); // Dispatches operations on net names
    void syncView(QPointF pos, qreal zoom); // Broadcast to all image views to sync
    void syncWaveformCursorPos(QString sid, uint index, uint pos); // Broadcast to all waveform views to sync the cursors
    void shutdown();                        // Application is shutting down; save all modified app data

private:
    ClassAnnotate m_annotate;   // Global annotations
    ClassVisual   m_chip;       // Global visual chip resource class
    ClassColors   m_colors;     // Global application colors
    ClassScript   m_script;     // Global scripting support
    ClassServer   m_server;     // Global socket server class
    ClassSimZ80   m_simz80;     // Global Z80 simulator class
    ClassWatch    m_watch;      // Global watchlist
    ClassTip      m_tips;       // Global tips
    ClassTrickbox m_trick;      // Global trickbox supporting environment
};

extern ClassController controller;

#endif // CLASSCONTROLLER_H
