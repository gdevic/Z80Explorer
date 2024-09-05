#include "ClassController.h"
#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QStringBuilder>

bool ClassController::init(QJSEngine *sc)
{
    qInfo() <<  "App init...";

    sc->globalObject().setProperty("control", sc->newQObject(this));
    sc->globalObject().setProperty("script", sc->newQObject(&m_script));
    sc->globalObject().setProperty("sim", sc->newQObject(&m_simz80));
    sc->globalObject().setProperty("monitor", sc->newQObject(&m_trick));

    // Java engine should not garbage collect or otherwise destruct these objects
    sc->setObjectOwnership(this, QJSEngine::CppOwnership);
    sc->setObjectOwnership(&m_script, QJSEngine::CppOwnership);
    sc->setObjectOwnership(&m_simz80, QJSEngine::CppOwnership);
    sc->setObjectOwnership(&m_trick, QJSEngine::CppOwnership);

    m_script.init(sc);

    connect(this, &ClassController::shutdown, &m_annotate, &ClassAnnotate::onShutdown);
    connect(this, &ClassController::shutdown, &m_colors, &ClassColors::onShutdown);
    connect(this, &ClassController::shutdown, &m_script, &ClassScript::stop);
    connect(this, &ClassController::shutdown, &m_simz80, &ClassSimZ80::onShutdown);
    connect(this, &ClassController::shutdown, &m_tips, &ClassTip::onShutdown);
    connect(this, &ClassController::shutdown, &m_watch, &ClassWatch::onShutdown);

    QSettings settings;
    QString resDir = settings.value("ResourceDir", QDir::currentPath()  + "/resource").toString();

#if HAVE_PREBUILT_LAYERMAP
    // Check if the current resource path contains required resource(s)
    qInfo() << "Checking for resource/layermap.bin";
    while (!QFile::exists(resDir + "/layermap.bin") && !QFile::exists(resDir + "/layermap.qz"))
    {
        // Prompts the user to select the chip resource folder
        QString fileName = QFileDialog::getOpenFileName(nullptr,
        "Select the application resource folder with layermap.bin or layermap.qz file", "layermap.*", "Any file (*.*)");
        if (!fileName.isEmpty())
            resDir = QFileInfo(fileName).path();
        else
            return false;
    }
    if (!QFile::exists(resDir + "/layermap.bin"))
    {
        // Attempt to uncompress it proceeding to load it
        if (!::controller.uncompressFile(resDir + "/layermap.qz", resDir + "/layermap.bin"))
            return false;
    }
    settings.setValue("ResourceDir", resDir);
#endif
    QDir::setCurrent(resDir);

    // Initialize all global classes using the given path to resource
    if (!m_simz80.loadResources(resDir) || !m_colors.load(resDir + "/colors.json") || !m_chip.loadChipResources(resDir) || !m_simz80.initChip())
    {
        qCritical() << "Unable to load chip resources from" << resDir;
        return false;
    }

    m_watch.load(resDir + "/watchlist.json");
    connect(this, &ClassController::eventNetName, &m_watch, &ClassWatch::onNetName);

    m_annotate.load(resDir + "/annotations.json");
    m_tips.load(resDir + "/tips.json");

    // Execute init.js initialization script
    QTimer::singleShot(1000, [=](){ m_script.exec( R"(load("init.js"))" ); } );

    return true;
}

/*
 * Runs the chip reset sequence, returns the number of clocks thet reset took
 */
uint ClassController::doReset()
{
    qDebug() << "Chip reset";
    m_watch.clear(); // Clear watch signal history
    m_trick.reset(); // Reset the control counters etc.
    uint hcycle = m_simz80.doReset();
    emit onRunStopped(hcycle);
    return hcycle;
}

/*
 * Runs the simulation for the given number of clocks
 */
void ClassController::doRunsim(uint ticks)
{
    m_simz80.doRunsim(ticks);

    if (ticks == INT_MAX)
        qInfo() << "Starting simulation";
    else if (ticks)
        qInfo() << "Simulation run for" << ticks << "half-clocks";
    else
        qInfo() << "Simulation stopped";
}

/*
 * Returns a list of formats applicable to the signal name (a net or a bus)
 */
const QStringList ClassController::getFormats(QString name)
{
    static const QStringList formats[2] = {
        { "Logic", "Transition Up", "Transition Down", "Transition Any" },
        { "Hexadecimal", "Binary", "Octal", "Decimal", "ASCII", "Disasm", "Ones' Complement" }
    };
    // If the name represents a bus, get() will return 0 for the net number, selecting formats[!0]
    return formats[!getNetlist().get(name)];
}

/*
 * Returns the formatted string for a bus type value
 * If decorated is true (default), the numbers will have Verilog-style prefix specifying the given width
 */
const QString ClassController::formatBus(uint fmt, uint value, uint width, bool decorated)
{
    if (Q_UNLIKELY(value == UINT_MAX)) return "hi-Z";
    if (fmt == FormatBus::OnesComplement)
        value = (~value) & ((1 << width) - 1);
    QString s = (decorated ? (QString::number(width) % "'h") : QString()) % QString::number(value, 16).toUpper(); // Print hex by default
    // Handle a special case where asked to print ASCII, but a value is not a prinable character: return its hex value
    if (Q_UNLIKELY((fmt == FormatBus::Ascii) && !QChar::isPrint(value)))
        return s;
    // Handle a special case where asked to print Disasm, but the width is not exactly 8 bits: return its hex value
    if (Q_UNLIKELY((fmt == FormatBus::Disasm) && (width != 8)))
        return s;
    static bool wasED = false;
    static bool wasCB = false;
    switch (fmt)
    {
        case FormatBus::Bin: s = (decorated ? (QString::number(width) % "'b") : QString()) % QString::number(value, 2);  break;
        case FormatBus::Oct: s = (decorated ? (QString::number(width) % "'o") : QString()) % QString::number(value, 8);  break;
        case FormatBus::Dec: s = (decorated ? (QString::number(width) % "'d") : QString()) % QString::number(value, 10); break;
        case FormatBus::Ascii: s = "'" % QChar(value) % "'"; break;
        case FormatBus::Disasm:
            // HACK: To get better opcode decode we keep the last ED/CB assuming we are called in-order
            s = z80state::disasm(value, !wasED, !wasCB);
            if (value != 0x00) // NOP keeps the flags as-is since there is a one-cycle NOP after ED/CB
                wasED = (value == 0xED), wasCB = (value == 0xCB);
            break;
    }
    if (fmt == FormatBus::OnesComplement)
        return "~" % s; // 1s-complement format shows tilde to accentuate it
    return s;
}

/*
 * Sets the name (alias) for a net
 */
void ClassController::setNetName(const QString name, const net_t net)
{
    m_simz80.eventNetName(Netop::SetName, name, net);
    emit eventNetName(Netop::SetName, name, net);
    emit eventNetName(Netop::Changed, QString(), net);
}

/*
 * Renames a net using the new name
 */
void ClassController::renameNet(const QString name, const net_t net)
{
    m_simz80.eventNetName(Netop::Rename, name, net);
    emit eventNetName(Netop::Rename, name, net);
    emit eventNetName(Netop::Changed, QString(), net);
}

/*
 * Deletes the current name of a specified net
 */
void ClassController::deleteNetName(const net_t net)
{
    m_simz80.eventNetName(Netop::DeleteName, QString(), net);
    emit eventNetName(Netop::DeleteName, QString(), net);
    emit eventNetName(Netop::Changed, QString(), net);
}

/*
 * Compresses a file, writing the result into another file
 */
bool ClassController::compressFile(const QString &inFileName, const QString &outFileName)
{
    QFile file(inFileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Could not open file for reading.";
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QByteArray compressedData = qCompress(fileData);

    QFile compressedFile(outFileName);
    if (!compressedFile.open(QIODevice::WriteOnly))
    {
        qWarning() << "Could not open file for writing.";
        return false;
    }

    QDataStream out(&compressedFile);
    out << compressedData;
    compressedFile.close();

    return true;
}

/*
 * Uncompresses a file, writing the result into another file
 */
bool ClassController::uncompressFile(const QString &inFileName, const QString &outFileName)
{
    QFile compressedFile(inFileName);
    if (!compressedFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Could not open file for reading.";
        return false;
    }

    QByteArray compressedData;
    QDataStream in(&compressedFile);
    in >> compressedData;
    compressedFile.close();

    QByteArray uncompressedData = qUncompress(compressedData);

    QFile uncompressedFile(outFileName);
    if (!uncompressedFile.open(QIODevice::WriteOnly))
    {
        qWarning() << "Could not open file for writing.";
        return false;
    }

    uncompressedFile.write(uncompressedData);
    uncompressedFile.close();

    return true;
}
