#include "ClassTrickbox.h"
#include <QDebug>
#include <QFile>

ClassTrickbox::ClassTrickbox(QObject *parent) : QObject(parent)
{
}

uint8_t ClassTrickbox::readMem(uint16_t ab)
{
    return m_mem[ab];
}

void ClassTrickbox::writeMem(uint16_t ab, uint8_t db)
{
    m_mem[ab] = db;
}

uint8_t ClassTrickbox::readIO(uint16_t ab)
{
    Q_UNUSED(ab);
    return 0;
}

void ClassTrickbox::writeIO(uint16_t ab, uint8_t db)
{
    if (ab != 8 * 256)
        return;
    if (db == 10)
        return;
    emit echo(char(db));
}

// https://en.wikipedia.org/wiki/Intel_HEX
bool ClassTrickbox::loadIntelHex(const QString fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open" << fileName;
        return false;
    }

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
            qDebug() << "Invalid starting character" << c;
            break;
        }
        uint8_t count = in.read(2).toUtf8().toUInt(&bStatus, 16);
        uint8_t addressH = in.read(2).toUtf8().toUInt(&bStatus, 16);
        uint8_t addressL = in.read(2).toUtf8().toUInt(&bStatus, 16);
        uint8_t type = in.read(2).toUtf8().toUInt(&bStatus, 16);
        if (type > 1) // 0 - "Data", 1 - "End Of file"
        {
            qDebug() << "Unexpected type" << type;
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
            qDebug() << "Checksum mismatch";
            break;
        }
    }
    file.close();
    if (in.atEnd())
        qDebug() << "Loaded" << fileName << "into RAM";

    return in.atEnd();
}
