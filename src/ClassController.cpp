#include "ClassController.h"
#include <QDebug>
#include <QFileDialog>
#include <QSettings>

bool ClassController::init()
{
    connect(&m_trick, SIGNAL(echo(char)), this, SIGNAL(echo(char)));
    connect(&m_simx, SIGNAL(runStopped()), this, SIGNAL(onRunStopped()));

    QSettings settings;
    QString path = settings.value("ChipResources", QDir::currentPath()).toString();

    // Check if the current resource path contains some of the resources
    while (!QFile::exists(path + "/nodenames.js"))
    {
        // Prompts the user to select the chip resource folder
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Select the application resource folder", "", "Any file (*.*)");
        if (!fileName.isEmpty())
            path = QFileInfo(fileName).path();
        else
            return false;
    }
    settings.setValue("ChipResources", path);

    // Initialize all global classes using the given path to resource
    if (!m_simx.loadResources(path) || !m_chip.loadChipResources(path) || !m_sim.loadSimResources(path))
    {
        qCritical() << "Unable to load chip resources from" << path;
        return false;
    }

    // Load the optional watchlist file
    QString fileWatchlist = settings.value("WatchlistFile", "").toString();
    m_watch.loadWatchlist(fileWatchlist);

    // Load the "hello world" sample executable file
    if (!m_trick.loadIntelHex(path + "/hello_world.hex"))
        qWarning() << "Unable to load example Z80 hex file";

    m_simx.initChip();

    return true;
}

/*
 * Runs chip reset sequence
 */
void ClassController::doReset()
{
    m_simx.doReset();
    m_watch.doReset();
    emit this->onRunStopped();
    qDebug() << "Chip reset";
}

/*
 * Controls the simulation
 */
void ClassController::doRunsim(uint ticks)
{
    emit m_simx.doRunsim(ticks);
    qDebug() << "Chip run for" << ticks << "half-clocks";
}
