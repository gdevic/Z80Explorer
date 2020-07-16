#include "ClassTrickbox.h"
#include "ClassController.h"
#include <QDebug>
#include <QFile>

#define TRICKBOX_START 0xD000
#define TRICKBOX_END   (TRICKBOX_START + sizeof(trick) - 1)

const static QStringList pins = { "_int", "_nmi", "_busrq", "_wait", "_reset" };

ClassTrickbox::ClassTrickbox(QObject *parent) : QObject(parent)
{
    static_assert(sizeof(trick) == (2 + 2 + 4 + 5*4), "unexpected trick struct size (packing?)");
    m_trick = (trick *)&m_mem[TRICKBOX_START];
    memset(m_mio, 0xFF, sizeof(m_mio));
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
    if (ab >= m_rom)
        m_mem[ab] = db;

    // Trickbox control address space
    if (m_trickEnabled && (ab >= TRICKBOX_START) && (ab <= TRICKBOX_END))
    {
        // Writing to the "stop" word immediately stops the simulation
        if ((ab & ~1) == TRICKBOX_START)
        {
            qInfo() << "Stopping sim: tb_stop";
            return ::controller.getSimZ80().doRunsim(0);
        }
        // We let the value be written in RAM first, which is a backing store for the trickbox
        // counters anyways; here we check the validity of cycle values but only on the second
        // memory access of each 2-byte word. Likewise, we disable trickbox control in between
        // writing the first byte (low value) and the final, second byte (high value).
        m_trickWriteEven = ab & 1;
        if (!m_trickWriteEven)
            return;

        uint current = ::controller.getSimZ80().getCurrentHCycle();

        for (uint i = 0; i < MAX_PIN_CTRL; i++) // { "_int", "_nmi", "_busrq", "_wait", "_reset" };
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
    return m_mio[ab];
}

/*
 * Writes to simulated IO space
 */
void ClassTrickbox::writeIO(quint16 ab, quint8 db)
{
    m_mio[ab] = db;
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
 * Reads sim monitor state as strings
 */
const QString ClassTrickbox::readState()
{
    QString s = QString("hcycle:%1\nstopAt:%2\n").arg(m_trick->curCycle).arg(m_trick->cycleStop);
    s += QString("breakWhen:%1 is %2\n").arg(m_bpnet).arg(m_bpval);
    for (int i = 0; i < MAX_PIN_CTRL; i++)
        s += QString("%1 at: %2 hold-for: %3\n").arg(pins[i],7).arg(m_trick->pinCtrl[i].cycle).arg(m_trick->pinCtrl[i].count);
    return s;
}

/*
 * Called by the simulator on every clock with the current half-clock upcounter value
 */
void ClassTrickbox::onTick(uint ticks)
{
    m_trick->curCycle = ticks;

    // Check for explicit break on a net value
    if (m_bpnet && (::controller.getSimZ80().getNetState(m_bpnet) == !!m_bpval))
    {
        qInfo() << "Trickbox: Break on a net value condition";
        return ::controller.getSimZ80().doRunsim(0);
    }

    if (!m_trickWriteEven)
        return;

    if ((m_trick->cycleStop > 0) && (m_trick->cycleStop == ticks))
    {
        qInfo() << "Trickbox: Stopping at cycle" << ticks;
        return ::controller.getSimZ80().doRunsim(0);
    }

    for (uint i = 0; i < MAX_PIN_CTRL; i++) // { "_int", "_nmi", "_busrq", "_wait", "_reset" };
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
 * Sets named pin to a value (0,1,2)
 * Value is optional and, if not given, it will be '0', which means "activate" for the NMOS input pads
 */
void ClassTrickbox::set(QString pin, quint8 value)
{
    int i = pins.indexOf(pin);
    if (i >= 0)
    {
        if (value <= 2)
        {
            ::controller.getSimZ80().setPin(i, value);
            emit refresh();
        }
        else
            qWarning() << "Invalid value. Permitted values are 0, 1 and 2";
    }
    else
        qWarning() << "Invalid pin name. Only input pads can be set:" << pins;
}

/*
 * Activates (sets to 0) named pin at the specified hcycle and holds it for the count number of cycles
 */
void ClassTrickbox::setAt(QString pin, quint16 hcycle, quint16 count)
{
    int i = pins.indexOf(pin);
    if (i >= 0)
    {
        m_trick->pinCtrl[i].cycle = hcycle;
        m_trick->pinCtrl[i].count = count;
        emit refresh();
    }
    else
        qWarning() << "Invalid pin name. Only input pads can be set:" << pins;
}

/*
 * Stops running when the given net number's state equals the value
 * If the net is 0, clears the last break setup
 */
void ClassTrickbox::breakWhen(quint16 net, quint8 value)
{
    if (value <= 1)
    {
        m_bpnet = net;
        m_bpval = value;
        emit refresh();
    }
    else
        qWarning() << "Invalid value. Permitted values are 0 or 1";
}

/*
 * Loads a HEX file into simulated RAM; empty name for last loaded
 *
 * https://en.wikipedia.org/wiki/Intel_HEX
 */
bool ClassTrickbox::loadHex(const QString fileName)
{    
    QString name = fileName.isEmpty() ? m_lastLoadedHex : fileName;
    if (QFileInfo(name).size() == 0)
    {
        qWarning() << "Empty file" << name;
        return false;
    }
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Unable to open" << name;
        return false;
    }

    // Clear the RAM memory and IO space before loading any programs
    memset(m_mem, 0, sizeof(m_mem));
    memset(m_mio, 0xFF, sizeof(m_mio));
    qInfo() << "Clearing simulator RAM and setting IO space to FF";

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

/*
 * Loads a binary file into simulated RAM at the given address
 */
bool ClassTrickbox::loadBin(const QString fileName, quint16 address)
{
    if (QFileInfo(fileName).size() == 0)
    {
        qWarning() << "Empty file" << fileName;
        return false;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Unable to open" << fileName;
        return false;
    }

    QByteArray blob = file.readAll();
    uint p = address;
    for (int i = 0; (i < blob.count()) && (p < 0x10000); i++, p++)
        m_mem[p] = blob[i];

    qInfo() << "Loaded" << fileName << "into simulator RAM at address" << address << "to" << p - 1;

    return true;
}

/*
 * Saves the content of the simulated RAM from the given address, up to size bytes
 */
bool ClassTrickbox::saveBin(const QString fileName, quint16 address, uint size)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "Unable to open" << fileName;
        return false;
    }

    if ((int(size) + address) > 0xFFFF)
        size = 0x10000 - address;
    QByteArray data((const char *)&m_mem[address], size);
    file.write(data);

    qInfo() << "Saved" << fileName << "from address" << address << "size" << size;

    return true;
}

#include <QWidget>

struct zx : public QWidget
{
    zx(uint8_t *a) : ram(a)
    {
        resize(512,384);
        QTimer *t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [=](){update();});
        t->start(640);
    };
    const QColor ink[8] {Qt::black, Qt::blue, Qt::red, Qt::magenta, Qt::green, Qt::cyan, Qt::yellow, Qt::white};
    uint8_t *ram;
    int f;
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this); f=!f;
        for (uint i=0; i<6144; i++)
        {
            uint y=((i>>8)&7)|((i>>2)&0x38)|((i>>5)&0xC0);
            uint x=(i&31)<<3;
            uint8_t b=ram[16384|i];
            uint8_t c=ram[22528|((y&0xF8)<<2)|(x>>3)];
            for (uint j=0; j<8; j++)
            {
                bool pix=((b>>(~j&7))&1) ^ ((c>>7)&f);
                QColor col = pix ? ink[c&7] : ink[(c>>3)&7];
                painter.setPen(col);
                painter.setBrush(col);
                painter.drawRect((x+j)*2, y*2, 2, 2);
            }
        }
    }
};

void ClassTrickbox::zx()
{
    struct zx *w = new struct zx(m_mem);
    w->show();
}
