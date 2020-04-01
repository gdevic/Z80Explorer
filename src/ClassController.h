#ifndef CLASSCONTROLLER_H
#define CLASSCONTROLLER_H

#include "ClassChip.h"
#include "ClassSim.h"
#include "ClassSimX.h"
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

private:
    ClassChip  m_chip;      // Global chip resource class
    ClassSimX  m_simx;      // Global simulator simx class
    ClassSim   m_sim;       // Global simulator sim class
    ClassWatch m_watch;     // Global watchlist
};

extern ClassController controller;

#endif // CLASSCONTROLLER_H
