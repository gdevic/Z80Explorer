#include "ClassController.h"
#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QStringBuilder>

bool ClassController::init(QScriptEngine *sc)
{
    qInfo() <<  "App init...";
    m_script.init(sc);

    connect(&m_trick, SIGNAL(echo(char)), this, SIGNAL(echo(char)));
    connect(&m_simx, SIGNAL(runStopped(uint)), this, SIGNAL(onRunStopped(uint)));

    connect(this, SIGNAL(shutdown()), &m_chip.annotate, SLOT(onShutdown()));
    connect(this, SIGNAL(shutdown()), &m_chip.tips, SLOT(onShutdown()));
    connect(this, SIGNAL(shutdown()), &m_watch, SLOT(onShutdown()));   
    connect(this, SIGNAL(shutdown()), &m_simx, SLOT(onShutdown()));

    QSettings settings;
    QString path = settings.value("ResourceDir", QDir::currentPath()  + "/resource").toString();

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
    if (!m_simx.loadResources(path) || !m_chip.loadChipResources(path))
    {
        qCritical() << "Unable to load chip resources from" << path;
        return false;
    }

    m_watch.load(path);
    connect(this, &ClassController::eventNetName, &m_watch, &ClassWatch::onNetName);

    // Load the "hello world" sample executable file
    if (!m_trick.loadIntelHex(path + "/hello_world.hex"))
        qWarning() << "Unable to load example Z80 hex file";

    m_simx.initChip();

    return true;
}

/*
 * Run the chip reset sequence, returns the number of clocks thet reset took
 */
uint ClassController::doReset()
{
    qDebug() << "Chip reset";
    m_watch.clear(); // Clear watch signal history
    m_trick.reset(); // Reset the control counters etc.
    uint hcycle = m_simx.doReset();
    emit onRunStopped(hcycle);
    return hcycle;
}

/*
 * Runs the simulation for the given number of clocks
 */
void ClassController::doRunsim(uint ticks)
{
    m_simx.doRunsim(ticks);
    if (ticks != INT_MAX)
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

/*
 * Sets the name (alias) for a net
 */
void ClassController::setNetName(const QString name, const net_t net)
{
    m_simx.eventNetName(Netop::SetName, name, net);
    emit eventNetName(Netop::SetName, name, net);
    emit eventNetName(Netop::Changed, QString(), net);
}

/*
 * Renames a net using the new name
 */
void ClassController::renameNet(const QString name, const net_t net)
{
    m_simx.eventNetName(Netop::Rename, name, net);
    emit eventNetName(Netop::Rename, name, net);
    emit eventNetName(Netop::Changed, QString(), net);
}

/*
 * Deletes the current name of a specified net
 */
void ClassController::deleteNetName(const net_t net)
{
    m_simx.eventNetName(Netop::DeleteName, QString(), net);
    emit eventNetName(Netop::DeleteName, QString(), net);
    emit eventNetName(Netop::Changed, QString(), net);
}
