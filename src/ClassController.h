#ifndef CLASSCONTROLLER_H
#define CLASSCONTROLLER_H

#include "ClassChip.h"
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
    bool init();            // Initialize controller classes and variables

public: // API
    ClassChip  &getChip()  { return m_chip; }   // Returns a reference to the chip class
    ClassSim   &getSim()   { return m_sim;  }   // Returns a reference to the sim class
    ClassSimX  &getSimx()  { return m_simx; }   // Returns a reference to the simx class
    ClassWatch &getWatch() { return m_watch; }  // Returns a reference to the watch class

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

public slots:
    void doRunsim(uint ticks);              // Controls the simulation

signals:
    void echo(char e);                      // Echo a character onto the virtual console

private:
    ClassChip  m_chip;      // Global chip resource class
    ClassSimX  m_simx;      // Global simulator simx class
    ClassSim   m_sim;       // Global simulator sim class
    ClassWatch m_watch;     // Global watchlist
    ClassTrickbox m_trick;  // Global trickbox supporting environment
};

extern ClassController controller;

#endif // CLASSCONTROLLER_H
