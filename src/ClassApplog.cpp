#include "ClassApplog.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QSettings>

using namespace std;

extern CAppLogHandler *applog; // Application logging subsystem

/*
 * Handler for both Qt messages and application messages.
 * The output is forked to application logger and the log window
 */
void appLogMsgHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QSettings settings;
    int logLevel = settings.value("AppLogLevel", 3).toInt();

    QString s1 = "File: " + (context.file ? QString(context.file) : "?");
    QString s2 = "Function: " + (context.function ? QString(context.function) : "?");
    // These are logging levels:
    // Log level:       3           2             1              0 (can't be disabled)
    // enum QtMsgType   QtDebugMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg
    switch (type)
    {
    case QtFatalMsg:
        applog->WriteLine(s1, LogVerbose_Error);
        applog->WriteLine(s2, LogVerbose_Error);
        applog->WriteLine(msg, LogVerbose_Error);
        break;
    case QtCriticalMsg:
        if (logLevel>=1)
            applog->WriteLine(msg, LogVerbose_Error);
        break;
    case QtWarningMsg:
        if (logLevel>=2)
            applog->WriteLine(msg, LogVerbose_Warning);
        break;
    case QtDebugMsg:
    default:
        if (logLevel>=3)
            applog->WriteLine(msg, LogVerbose_Info);
        break;
    }
}

CAppLogHandler::CAppLogHandler(char* logname, int logoption)
{
    m_log_name = logname;
    m_log_options = logoption;

    if (CheckBit(m_log_options, LogOptions_File))
        InitLogFile();
}

CAppLogHandler::~CAppLogHandler()
{
    if (flog.is_open())
        flog.close();
}

QString CAppLogHandler::GetCurrentAppDirectory()
{
    QDir CurDir;
    return CurDir.absolutePath();
}

/*
 * Set new log options
 */
void CAppLogHandler::SetLogOptions(int logoption)
{
    m_log_options = logoption;
    if (CheckBit(m_log_options, LogOptions_File))
        InitLogFile();
    else
    {
        if (flog.is_open())
            flog.close();
    }
}

/*
 * Set new log file information
 */
void CAppLogHandler::SetNewLogFile(char* filepath, char* filename, int maxlogfilesize)
{
    if (filepath != nullptr)
        m_log_file_path = filepath;

    if (filename != nullptr)
        m_log_file_name = filename;

    m_max_log_file_size = maxlogfilesize;
    m_log_options |= LogOptions_File;

    InitLogFile();
}

/*
 * Initializing the log file.
 *   1. Close previously opened the file
 *   2. If the file size is greater than max size, rename the old one and create new one.
 *   3. Open file as output and append mode and add header to indicate the log name.
 */
void CAppLogHandler::InitLogFile()
{
    if (flog.is_open())
        flog.close();

    QString file = m_log_file_path + "/" + m_log_file_name;
    file = QDir::toNativeSeparators(file);

    if (CheckBit(m_log_options, LogOptions_Append))
    {
        // Check file exist, if so, check for size
        QFile logFile(file);
        if (logFile.exists())
        {
            qint64 size = logFile.size();
            if (size/1024 > m_max_log_file_size)
            {
                QString newfilename = file + "." + GetCurrentTimeFileString() + ".txt";
                newfilename = QDir::toNativeSeparators(newfilename);
                logFile.rename(newfilename);
            }
        }
        logFile.close();

        flog.open(file.toStdString().c_str(),  ios::out | ios::app);
    }
    else
        flog.open(file.toStdString().c_str());

    if (flog.is_open())
        flog << "-------- " << m_log_name.toStdString() << " --------\n\n";
}

QString CAppLogHandler::GetCurrentTimeString()
{
    QDateTime dateTime = QDateTime::currentDateTime();
    return dateTime.toString("yyyy-MM-dd hh:mm:ss");
}

QString CAppLogHandler::GetCurrentTimeFileString()
{
    QDateTime dateTime = QDateTime::currentDateTime();
    return dateTime.toString("yyyy-MM-dd hh-mm-ss");
}

/*
 * Write to log
 */
void CAppLogHandler::Write(char* message, int verbose)
{
    Q_UNUSED(verbose)
    QString logmessage = message;
    if (CheckBit(m_log_options, LogOptions_Signal))
        emit NewLogMessage(logmessage, false);
}

/*
 * Write line of log
 */
void CAppLogHandler::WriteLine(const QString message, int verbose)
{
    QString logmessage = GetCurrentTimeString() + " | " + verbose + " | " + message;
    if (verbose == LogVerbose_Command)
        logmessage = message;
    if (CheckBit(m_log_options, LogOptions_File))
    {
        if (flog.is_open())
        {
            AcquireLock();
            flog << logmessage.toStdString() << endl;
            ReleaseLock();
        }
        else
            cerr << "Log File is not present" << endl;
    }
    if (CheckBit(m_log_options, LogOptions_Shell))
        cout << logmessage.toStdString() << endl;
    if (CheckBit(m_log_options, LogOptions_Signal))
        emit NewLogMessage(logmessage, true);
    if (CheckBit(m_log_options, LogOptions_DebugOut))
        qDebug() << logmessage;
}

void CAppLogHandler::WriteLine(char* message, int verbose)
{
    WriteLine(QString(message), verbose);
}
