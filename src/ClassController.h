#ifndef CLASSCONTROLLER_H
#define CLASSCONTROLLER_H

#include "AppTypes.h"
#include "ClassAnnotate.h"
#include "ClassRenderer.h"
#include "ClassVisual.h"
#include "ClassColors.h"
#include "ClassScript.h"
#include "ClassServer.h"
#include "ClassSimZ80.h"
#if USE_AVX2_SIM
#include "ClassSimZ80_AVX2.h"
#endif
#include "ClassTip.h"
#include "ClassTrickbox.h"
#include "ClassWatch.h"
#include <QVector>
#include <functional>

class ClassMcpServer;
class ClassMcpTools;

/*
 * Controller class implements the controller pattern, handling all requests
 */
class ClassController : public QObject
{
    Q_OBJECT
public:
    explicit ClassController() {};
    bool init(QJSEngine *);                     // Initialize controller classes and variables
    void stopServers();                         // Tear down long-running background servers (MCP, SOCKET)
    void applyServerSettings();                 // Runtime reconciliation of server enable/port

    // What became of one item. A skip is not a failure: the item decided it had nothing worth
    // writing, which is how an empty waveform view avoids blanking a good configuration on disk.
    enum SaveOutcome { SaveWritten, SaveSkipped, SaveFailed };

    // One entry in the save registry. The registry is the single list of what the application can
    // write to disk; the save dialog, the script API and the MCP tool all read it, so a new kind of
    // user data becomes savable everywhere by adding one row in init().
    struct SaveItem
    {
        QString id;                             // Stable id used by the dialog, scripts and MCP
        QString name;                           // Display name
        std::function<QStringList()> files;     // Current target paths; each class caches its own
        // Writes the item. `automatic` marks the unattended save on the way out, where an item may
        // decline what it would write on an explicit request. Null while nothing backs this item.
        std::function<SaveOutcome(bool automatic, QString &reason)> save;
    };

    // The outcome of saving one item
    struct SaveResult
    {
        QString id;
        QStringList files;                      // Paths written; empty unless outcome is SaveWritten
        SaveOutcome outcome {SaveFailed};
        QString reason;                         // Why it was skipped or failed; empty when written
        bool ok() const { return outcome == SaveWritten; }
    };

public: // API
    inline ClassAnnotate &getAnnotation() { return m_annotate; }  // Returns a reference to the annotations class
    inline ClassVisual   &getChip()       { return m_chip; }      // Returns a reference to the chip class
    inline ClassRenderer &getRenderer()   { return m_renderer; }  // Offscreen die renderer (no widget needed)
    inline ClassColors   &getColors()     { return m_colors; }    // Returns a reference to the colors class
    inline ClassScript   &getScript()     { return m_script; }    // Returns a reference to the script class
    inline ClassServer   &getServer()     { return m_server; }    // Returns a reference to the server class
#if USE_AVX2_SIM
    inline ClassSimZ80_AVX2 &getSimZ80()  { return m_simz80avx2; } // Returns a reference to the AVX2 optimized Z80 simulator
#else
    inline ClassSimZ80   &getSimZ80()     { return m_simz80; }    // Returns a reference to the Z80 simulator class
#endif
    inline ClassWatch    &getWatch()      { return m_watch; }     // Returns a reference to the watch class
    inline ClassNetlist  &getNetlist()    { return m_simz80; }    // Returns a reference to the netlist class (always original for compatibility)
    inline ClassTip      &getTip()        { return m_tips; }      // Returns a reference to the tips class
    inline ClassTrickbox &getTrickbox()   { return m_trick; }     // Returns a reference to the Trickbox class
    inline ClassMcpTools *getMcpTools()   { return m_mcpTools; }  // Returns the MCP tool registry (nullptr if MCP disabled)
    inline ClassMcpServer*getMcpServer()  { return m_mcpServer; } // Returns the MCP server (nullptr if MCP disabled)
    // The save registry. User data can be written at any point in a session.
    inline const QVector<SaveItem> &saveItems() const { return m_saveItems; } // Every item, available or not
    // Saves the named items, or every available item. `automatic` is set only by the save on the
    // way out, so an item can tell an unattended write from one the user or an agent asked for.
    QVector<SaveResult> save(const QStringList &ids = {}, bool automatic = false);
    void attachSaveItem(const QString &id, std::function<QStringList()> files, std::function<SaveOutcome(bool automatic, QString &reason)> save);
    void detachSaveItem(const QString &id);       // Returns an item to its unavailable state

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
#if USE_AVX2_SIM
        { m_simz80avx2.readState(state); }
#else
        { m_simz80.readState(state); }
#endif
    bool isTransOn(tran_t t)                      // Returns true if a transistor is ON
#if USE_AVX2_SIM
        { return m_simz80avx2.isTransOn(t); }
#else
        { return m_simz80.isTransOn(t); }
#endif
    // Returns a net's state and its forced high / low levels from the simulator that runs, for the same reason
    void getNetDynamics(net_t n, bool &state, bool &isHigh, bool &isLow)
#if USE_AVX2_SIM
        { m_simz80avx2.getNetDynamics(n, state, isHigh, isLow); }
#else
        { m_simz80.getNetDynamics(n, state, isHigh, isLow); }
#endif
    bool isSimRunning()                           // Returns true is the simulation is currently running
#if USE_AVX2_SIM
        { return m_simz80avx2.isRunning(); }
#else
        { return m_simz80.isRunning(); }
#endif

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

signals:
    void onRunStarting(uint);               // Called by the sim when it is starting the simulation
    void onRunHeartbeat(uint);              // Called by the sim every 500ms when running
    void onRunStopped(uint);                // Called by the sim when the current run stops at a given half-cycle
    void eventNetName(Netop op, const QString name, const net_t); // Dispatches operations on net names
    void syncView(QPointF pos, qreal zoom); // Broadcast to all image views to sync
    void syncWaveformCursorPos(QString sid, uint index, uint pos); // Broadcast to all waveform views to sync the cursors
    void shutdown();                        // Application is shutting down; save all modified app data

private:
    ClassAnnotate m_annotate;               // Global annotations
    ClassRenderer m_renderer;               // Offscreen die renderer, used by the MCP render tool
    ClassVisual   m_chip;                   // Global visual chip resource class
    ClassColors   m_colors;                 // Global application colors
    ClassScript   m_script;                 // Global scripting support
    ClassServer   m_server;                 // Global socket server class
    ClassSimZ80   m_simz80;                 // Global Z80 simulator class (always needed for netlist)
#if USE_AVX2_SIM
    ClassSimZ80_AVX2 m_simz80avx2;          // AVX2 optimized Z80 simulator class
#endif
    ClassWatch    m_watch;                  // Global watchlist
    ClassTip      m_tips;                   // Global tips
    ClassTrickbox m_trick;                  // Global trickbox supporting environment
    ClassMcpTools  *m_mcpTools  {};         // MCP tool registry (lazily created on first MCP-server enable)
    ClassMcpServer *m_mcpServer {};         // MCP HTTP+JSON-RPC transport (lazily created; kept around once allocated)
    QVector<SaveItem> m_saveItems;          // Save registry, built in init()
    void startSocketServer(quint16 port);   // Start the socket server and wire its commandReceived handler
};

extern ClassController controller;

#endif // CLASSCONTROLLER_H
