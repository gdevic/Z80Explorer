#ifndef CLASSTRICKBOX_H
#define CLASSTRICKBOX_H

#include <QObject>

/*
 * This class implements the supporting environment for the simulation
 */
class ClassTrickbox : public QObject
{
    Q_OBJECT
public:
    explicit ClassTrickbox(QObject *parent = nullptr);

    bool loadIntelHex(const QString fileName); // Loads RAM memory with the content of an Intel HEX file

    uint8_t readMem(uint16_t ab);           // Read environment RAM
    void writeMem(uint16_t ab, uint8_t db); // Write environment RAM
    uint8_t readIO(uint16_t ab);            // Read environment IO
    void writeIO(uint16_t ab, uint8_t db);  // Write environment IO

signals:
    void echo(char c);                      // Character to a terminal

private:
    bool readHex(QString fileName);
    uint8_t m_mem[65536] {};                // Simulated 64K memory
};

#endif // CLASSTRICKBOX_H
