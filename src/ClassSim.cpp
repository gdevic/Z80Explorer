#include "ClassSim.h"
#include "Z80_Simulator.h"

#include <QDebug>
#include <QDir>

/*
 * Loads sim resources (netlist)
 */
bool ClassSim::loadSimResources(const QString dir)
{
    qInfo() << "Loading netlist from" << dir;
    QString file = dir + "/z80.netlist";
    if (QFileInfo::exists(file) && QFileInfo(file).isFile())
    {
        sim.simLoadNetlist(file.toUtf8().constData());

        qInfo() << "Completed loading netlist";
        return true;
    }
    qWarning() << "Unable to load \"z80.netlist\" file";
    return false;
}

/*
 * Run simulation using the Z80_Simulator code
 */
void ClassSim::onRun()
{
    sim.simulate();
}

/*
 * Stop simulation run
 */
void ClassSim::onStop()
{
    sim.stop();
}
