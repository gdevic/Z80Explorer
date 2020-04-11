#include "ClassController.h"
#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QStringBuilder>

bool ClassController::init()
{
    connect(&m_trick, SIGNAL(echo(char)), this, SIGNAL(echo(char)));
    connect(&m_simx, SIGNAL(runStopped()), this, SIGNAL(onRunStopped()));

    QSettings settings;
    QString path = settings.value("ResourceDir", QDir::currentPath()).toString();

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
    settings.setValue("ResourceDir", path);

    // Initialize all global classes using the given path to resource
    if (!m_simx.loadResources(path) || !m_chip.loadChipResources(path) || !m_sim.loadSimResources(path))
    {
        qCritical() << "Unable to load chip resources from" << path;
        return false;
    }

    // Load the watchlist file: use default or the last recently loaded/saved file
    QString fileWatchlist = settings.value("WatchlistFile", path + "/watchlist.wl").toString();
    settings.setValue("WatchlistFile", fileWatchlist); // Make sure the settings contains a valid entry
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
    m_watch.clear();
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

/*
 * Returns a list of formats applicable to the signal name (a net or a bus)
 */
const QStringList ClassController::getFormats(QString name)
{
    static const QStringList formats[2] = {
        { "Logic", "Transition Up", "Transition Down", "Transition Any" },
        { "Hexadecimal", "Binary", "Octal", "Decimal", "ASCII" }
    };
    // If the name represents a bus, get() will return 0 for the net number, selecting formats[!0]
    return formats[!getNetlist().get(name)];
}

/*
 * Returns the formatted string for a bus type value
 */
const QString ClassController::formatBus(uint fmt, uint value, uint width)
{
    QString s = QString::number(width) % "'h" % QString::number(value, 16); // Print hex by default
    // Handle a special case where asked to print ASCII, but a value is not a prinable character: return its hex value
    if ((fmt == FormatBus::Ascii) && !QChar::isPrint(value))
        return s;
    switch (fmt)
    {
        case FormatBus::Bin: s = QString::number(width) % "'b" % QString::number(value, 2); break;
        case FormatBus::Oct: s = QString::number(width) % "'o" % QString::number(value, 8); break;
        case FormatBus::Dec: s = QString::number(width) % "'d" % QString::number(value, 10); break;
        case FormatBus::Ascii: s = QString(QString::number(width) % "'" % QChar(value) % "'"); break;
    }
    return s;
}
