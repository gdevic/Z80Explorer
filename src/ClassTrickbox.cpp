#include "ClassTrickbox.h"
#include "ClassController.h"
#include <assert.h>
#include <QDebug>
#include <QFile>

#define TRICKBOX_START 0xD000
#define TRICKBOX_END   (TRICKBOX_START + sizeof(trick) - 1)

ClassTrickbox::ClassTrickbox(QObject *parent) : QObject(parent)
{
    static_assert(sizeof(trick) == (2 + 5*4 + 4), "unexpected trick struct size (packing?)");
    m_trick = (trick *)&m_mem[TRICKBOX_START];
}

void ClassTrickbox::reset()
{
    memset(m_trick, 0, sizeof (trick));
    for (uint i = 0; i < 5; i++)
        m_trick->pinCtrl[i].count = 6;
}

/*
 * Reads from simulated RAM
 */
quint8 ClassTrickbox::readMem(quint16 ab)
{
    return m_mem[ab];
}

/*
 * Writes to simulated RAM
 */
void ClassTrickbox::writeMem(quint16 ab, quint8 db)
{
    // Writing to address 0 immediately stops the simulation
    if (ab == 0)
    {
        qInfo() << "Stopping sim: WR(0)";
        return ::controller.getSimZ80().doRunsim(0);
    }
    m_mem[ab] = db;

    // Trickbox control address space
    if ((ab >= TRICKBOX_START) && (ab <= TRICKBOX_END))
    {
        const static QStringList pins = { "_int", "_nmi", "_busrq", "_wait", "_reset" };

        // We let the value already be written in RAM, which is a backing store for the trickbox
        // counters anyways; here we check the validity of cycle values but only on the second
        // memory access of each 2-byte word. Likewise, we disable trickbox control in between
        // writing the first byte (low value) and the final, second byte (high value).
        m_enableTrick = ab & 1;
        if (!m_enableTrick)
            return;

        uint current = ::controller.getSimZ80().getCurrentHCycle();

        for (uint i = 0; i < 5; i++) // { "_int", "_nmi", "_busrq", "_wait", "_reset" };
        {
            if (m_trick->pinCtrl[i].cycle && (m_trick->pinCtrl[i].cycle <= current)) // Zero is non-active
            {
                qInfo() << "Trickbox: Asked to assert pin " << pins[i] << "at hcycle"
                        << m_trick->pinCtrl[i].cycle << "but current is already at" << current;
                return ::controller.getSimZ80().doRunsim(0);
            }
        }
        if (m_trick->cycleStop)
        {
            if (m_trick->cycleStop <= current)
            {
                qInfo() << "Trickbox: Asked to stop sim at hcycle" << m_trick->cycleStop << "but current is already at" << current;
                return ::controller.getSimZ80().doRunsim(0);
            }
        }
    }
}

/*
 * Reads from simulated IO space
 */
quint8 ClassTrickbox::readIO(quint16 ab)
{
    Q_UNUSED(ab);
    return 0; // Read from any IO address returns 0x00
}

/*
 * Writes to simulated IO space
 */
void ClassTrickbox::writeIO(quint16 ab, quint8 db)
{
    // Terminal write address
    if (ab == 0x0800)
    {
        if (db == 10) // Ignore LF in CR/LF sequence
            return;
        if (db == 0x04) // Console output of character 4 (EOD, End-of-Transmission): stops the simulation
        {
            qInfo() << "Stopping simulation: CHAR(EOD)";
            ::controller.getSimZ80().doRunsim(0);
        }
        else
            emit echo(char(db));
    }
}

/*
 * Called by the simulator on every clock with the current half-clock upcounter value
 */
void ClassTrickbox::onTick(uint ticks)
{
    m_trick->curCycle = ticks;

    if (!m_enableTrick)
        return;

    if ((m_trick->cycleStop > 0) && (m_trick->cycleStop == ticks))
    {
        qInfo() << "Trickbox: Stopping at cycle" << ticks;
        return ::controller.getSimZ80().doRunsim(0);
    }

    for (uint i = 0; i < 5; i++) // { "_int", "_nmi", "_busrq", "_wait", "_reset" };
    {
        if ((m_trick->pinCtrl[i].cycle == 0) || (m_trick->pinCtrl[i].cycle > ticks))
            continue;
        if (m_trick->pinCtrl[i].cycle == ticks)
            ::controller.getSimZ80().setPin(i, 0); // and assert its pin if the cycle is reached
        else
        {
            if (m_trick->pinCtrl[i].count > 0)
                m_trick->pinCtrl[i].count--;
            if (m_trick->pinCtrl[i].count == 0)
            {
                ::controller.getSimZ80().setPin(i, 1); // Release the pin
                m_trick->pinCtrl[i].cycle = 0;
            }
        }
    }
}

/*
 * Loads a HEX file into simulated RAM; empty name for last loaded
 *
 * https://en.wikipedia.org/wiki/Intel_HEX
 */
bool ClassTrickbox::loadHex(const QString fileName)
{    
    QString name = fileName.isEmpty() ? m_lastLoadedHex : fileName;
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Unable to open" << name;
        return false;
    }

    // Clear the RAM memory before loading any programs
    memset(m_mem, 0, sizeof(m_mem));
    qInfo() << "Clearing simulator RAM";

    QTextStream in(&file);
    while (!in.atEnd())
    {
        bool bStatus;
        in.skipWhiteSpace();
        if (in.atEnd())
            break;
        QString c = in.read(1);
        if (c != ':')
        {
            qWarning() << "Invalid starting character" << c;
            break;
        }
        uint8_t count = in.read(2).toUtf8().toUInt(&bStatus, 16);
        uint8_t addressH = in.read(2).toUtf8().toUInt(&bStatus, 16);
        uint8_t addressL = in.read(2).toUtf8().toUInt(&bStatus, 16);
        uint8_t type = in.read(2).toUtf8().toUInt(&bStatus, 16);
        if (type > 1) // 0 - "Data", 1 - "End Of file"
        {
            qWarning() << "Unexpected type" << type;
            break;
        }
        uint16_t sum = count + addressL + addressH + type;
        uint16_t address = (uint16_t(addressH) << 8) + addressL;
        while (count--)
        {
            uint8_t byte = in.read(2).toUtf8().toUInt(&bStatus, 16);
            m_mem[address++] = byte;
            sum += byte;
        }
        uint16_t checksum = in.read(2).toUtf8().toUInt(&bStatus, 16);
        if ((checksum + sum) & 0xFF)
        {
            qWarning() << "Checksum mismatch";
            break;
        }
    }
    if (in.atEnd())
    {
        qInfo() << "Loaded" << name << "into simulator RAM";
        m_lastLoadedHex = name;
        return true;
    }
    return false;
}
