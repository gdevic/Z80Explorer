#ifndef CLASSASSEMBLER_H
#define CLASSASSEMBLER_H

#include <QObject>
#include <QString>

/*
 * Synchronous wrapper around the bundled zmac binary at <ResourceDir>/tests/zmac.<platform-ext>.
 * Used by DockMonitor to assemble a dropped .asm into Intel HEX before falling through to the
 * existing loadHex path. zmac is launched with the .asm's absolute path and the resource/tests/
 * directory as its working directory, so any `include trickbox.inc` resolves the same way it
 * does for the bundled test programs. The produced .hex is copied next to the dropped .asm.
 */
class ClassAssembler : public QObject
{
    Q_OBJECT
public:
    explicit ClassAssembler(QObject *parent = nullptr);

    // Assemble asmPath using the bundled zmac. On success outHexPath holds the absolute path of
    // the produced .hex (placed in the same directory as the .asm, basename + .hex). On failure
    // errorText holds zmac's stderr/stdout (or a missing-binary / I/O error). Returns true on
    // success.
    bool assemble(const QString &asmPath, QString &outHexPath, QString &errorText);

private:
    // Returns absolute path of the platform-appropriate zmac binary if it exists, else empty.
    // expectedPath (if non-null) receives the path we looked for, even when not found.
    static QString resolveZmacPath(QString *expectedPath = nullptr);
};

#endif // CLASSASSEMBLER_H
