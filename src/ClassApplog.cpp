#include "ClassApplog.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>

using namespace std;

CAppLogHandler::CAppLogHandler()
{
    m_log_file_path = GetCurrentAppDirectory();
    m_log_file_name = DEFAULT_APP_LOG_FILE_NAME;
    m_max_log_file_size = DEFAULT_APP_LOG_FILE_SIZE;
    m_cur_stream_output_verbose = LogVerbose_Info;
}

CAppLogHandler::CAppLogHandler(char* logname, int logoption)
{
    m_log_file_path = GetCurrentAppDirectory();
    m_log_file_name = DEFAULT_APP_LOG_FILE_NAME;
    m_max_log_file_size = DEFAULT_APP_LOG_FILE_SIZE;

    m_log_name = logname;
    m_log_options = logoption;

    if (CheckBit(m_log_options, LogOptions_File))
    {
        InitLogFile();
    }
}

CAppLogHandler::~CAppLogHandler()
{
    if (flog.is_open())
    {
        flog.close();
    }
}

void CAppLogHandler::AcquireLock()
{
    m_file_lock.lock();
}

void CAppLogHandler::ReleaseLock()
{
    m_file_lock.unlock();
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
    if (filepath != NULL)
        m_log_file_path = filepath;

    if (filename != NULL)
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
    QDateTime dateTime = dateTime.currentDateTime();
    return dateTime.toString("yyyy-MM-dd hh:mm:ss");
}

QString CAppLogHandler::GetCurrentTimeFileString()
{
    QDateTime dateTime = dateTime.currentDateTime();
    return dateTime.toString("yyyy-MM-dd hh-mm-ss");
}

/*
 * Write to log
 */
void CAppLogHandler::Write(char* message, int verbose)
{
    QString logmessage = message;
    if (CheckBit(m_log_options, LogOptions_Signal))
    {
        emit NewLogMessage(logmessage, false);
    }
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

/*
 * Operator << overload to change the message type
 */
CAppLogHandler& CAppLogHandler::operator<<(LogVerbose t)
{
    m_cur_stream_output_verbose = t;
    return *this;
}

/*
 * Operator << overload to output const chars
 */
CAppLogHandler& CAppLogHandler::operator<<(const char* t)
{
    QTextStream ts(&m_message);
    ts << t;
    if (t[strlen(t)-1] == '\n')
    {
        m_message.remove(m_message.length()-1, 1);  // remove last char because writeline will write it
        WriteLine((char*) m_message.toStdString().c_str(), m_cur_stream_output_verbose);
        m_message.clear();
    }
    return *this;
}

/*
 * Operator << overload to output QString
 */
CAppLogHandler& CAppLogHandler::operator<<(const QString & t)
{
    QTextStream ts(&m_message);
    ts << t;
    if (t[t.length()-1] == '\n')
    {
        m_message.remove(m_message.length()-1, 1);  // remove last char because writeline will write it
        WriteLine((char*) m_message.toStdString().c_str(), m_cur_stream_output_verbose);
        m_message.clear();
    }
    return *this;
}

/*
 * Operator << overload to output std::endl
 */
CAppLogHandler& CAppLogHandler::operator<<(std::ostream&(*t)(std::ostream&) )
{
    typedef std::ostream& (*os_t)(std::ostream&);

    if (t == static_cast<os_t>(std::endl))
    {
        WriteLine((char*) m_message.toStdString().c_str(), m_cur_stream_output_verbose);
        m_message.clear();
    }
    return *this;
}
