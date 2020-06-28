#ifndef CLASS_APPLOG_H
#define CLASS_APPLOG_H

#include <QMutex>
#include <QObject>
#include <QString>
#include <fstream>
#include <iostream>
#include <qtextstream.h>

#define DEFAULT_APP_LOG_FILE_SIZE       1000 // in KBbytes
#define DEFAULT_APP_LOG_FILE_NAME       "z80explorer.log"

extern void appLogMsgHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

enum LogOptions
{
    LogOptions_File = 0x01,
    LogOptions_Shell = 0x02,
    LogOptions_Signal = 0x04,
    LogOptions_DebugOut = 0x08,
    LogOptions_Append = 0x10,
    LogOptions_Enable_All = 0xFFFFFFFF
};

enum LogVerbose
{
    LogVerbose_Info = 'I',              // Information
    LogVerbose_Warning = 'W',           // Warning message
    LogVerbose_Error = 'E',             // Error message
    LogVerbose_Command = 'C'            // Command
};

class CAppLogHandler : public QObject
{
    Q_OBJECT

public:
    CAppLogHandler() {};
    CAppLogHandler(char* logname, int logoption);
    ~CAppLogHandler();

    virtual void WriteLine(char* message, int verbose = LogVerbose_Info);
    virtual void WriteLine(const QString message, int verbose = LogVerbose_Info);
    void Write(char* message, int verbose);
    void SetNewLogFile(char* filepath, char* filename, int maxlogfilesize = DEFAULT_APP_LOG_FILE_SIZE);
    int GetLogOptions() { return m_log_options; }
    void SetLogOptions(int logoption);
    void SetLogName(char* logname) { m_log_name = logname;}

signals:
     void NewLogMessage(QString message, bool newline);

protected:
    std::ofstream flog;
    QString m_log_name { "Z80 Explorer" };
    QString m_log_file_path { GetCurrentAppDirectory() };
    QString m_log_file_name { DEFAULT_APP_LOG_FILE_NAME };
    QString m_message;
    int m_cur_stream_output_verbose { LogVerbose_Info };

    int m_log_options {};
    int m_max_log_file_size { DEFAULT_APP_LOG_FILE_SIZE };
    QMutex m_file_lock;

    void AcquireLock() { m_file_lock.lock(); }
    void ReleaseLock() { m_file_lock.unlock(); }
    void InitLogFile();
    QString GetCurrentAppDirectory();
    QString GetCurrentTimeString();
    QString GetCurrentTimeFileString();
};

#endif // CLASS_APPLOG_H
