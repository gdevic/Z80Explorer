#include "ClassAssembler.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStringBuilder>

ClassAssembler::ClassAssembler(QObject *parent) : QObject(parent) {}

QString ClassAssembler::resolveZmacPath(QString *expectedPath)
{
    QSettings settings;
    const QString resDir = settings.value("ResourceDir").toString();
    if (resDir.isEmpty())
    {
        if (expectedPath) *expectedPath = QStringLiteral("(ResourceDir not set)");
        return QString();
    }

#if defined(Q_OS_WIN)
    const QString name = QStringLiteral("zmac.exe");
#elif defined(Q_OS_MACOS)
    const QString name = QStringLiteral("zmac.macos");
#elif defined(Q_OS_LINUX)
    const QString name = QStringLiteral("zmac.linux");
#else
    if (expectedPath) *expectedPath = QStringLiteral("(unsupported OS)");
    return QString();
#endif

    const QString path = resDir % QStringLiteral("/tests/") % name;
    if (expectedPath) *expectedPath = path;
    return QFile::exists(path) ? path : QString();
}

bool ClassAssembler::assemble(const QString &asmPath, QString &outHexPath, QString &errorText)
{
    outHexPath.clear();
    errorText.clear();

    QString expectedZmac;
    const QString zmac = resolveZmacPath(&expectedZmac);
    if (zmac.isEmpty())
    {
        errorText = QStringLiteral("zmac binary not found at ") % expectedZmac;
        return false;
    }

    QSettings settings;
    const QString resDir = settings.value("ResourceDir").toString();
    const QString workDir = resDir % QStringLiteral("/tests");

    const QFileInfo asmInfo(asmPath);
    if (!asmInfo.exists())
    {
        errorText = QStringLiteral("Source not found: ") % asmPath;
        return false;
    }
    const QString asmAbs = asmInfo.absoluteFilePath();
    const QString baseName = asmInfo.completeBaseName();   // foo.bar.asm -> foo.bar (matches zmac)

    // Run zmac with the same flags as resource/tests/make_test.bat / make_test.sh. Working dir is
    // resource/tests so `include trickbox.inc` resolves; the .asm is passed by absolute path.
    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(zmac, QStringList{ QStringLiteral("--zmac"),
                                  QStringLiteral("--oo"),
                                  QStringLiteral("hex,bds,lst"),
                                  asmAbs });
    if (!proc.waitForStarted(5000))
    {
        errorText = QStringLiteral("zmac failed to start: ") % proc.errorString();
        return false;
    }
    if (!proc.waitForFinished(60000))
    {
        proc.kill();
        proc.waitForFinished(2000);
        errorText = QStringLiteral("zmac timed out after 60 s");
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
    {
        errorText = QString::fromLocal8Bit(proc.readAll()).trimmed();
        if (errorText.isEmpty())
            errorText = QStringLiteral("zmac exited with code %1").arg(proc.exitCode());
        return false;
    }

    // zmac writes zout/<base>.hex relative to its working directory.
    const QString producedHex = workDir % QStringLiteral("/zout/") % baseName % QStringLiteral(".hex");
    if (!QFile::exists(producedHex))
    {
        errorText = QStringLiteral("zmac succeeded but ") % producedHex % QStringLiteral(" was not created");
        return false;
    }

    // Copy the .hex next to the dropped .asm. Overwrite if a previous .hex was there.
    const QString destHex = asmInfo.absolutePath() % QStringLiteral("/") % baseName % QStringLiteral(".hex");
    if (QFileInfo(producedHex).canonicalFilePath() == QFileInfo(destHex).canonicalFilePath())
    {
        // .asm already lives in resource/tests/zout — extremely unlikely but be safe.
        outHexPath = destHex;
        return true;
    }
    QFile::remove(destHex);
    if (!QFile::copy(producedHex, destHex))
    {
        errorText = QStringLiteral("Could not copy ") % producedHex % QStringLiteral(" to ") % destHex;
        return false;
    }

    outHexPath = destHex;
    return true;
}
