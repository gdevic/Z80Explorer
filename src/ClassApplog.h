#ifndef CLASS_APPLOG_H
#define CLASS_APPLOG_H

#include <iostream>
#include <fstream>
#include <QMutex>
#include <QString>
#include <QObject>
#include <qtextstream.h>

#define DEFAULT_APP_LOG_FILE_SIZE       1000   // in KBbytes
#define DEFAULT_APP_LOG_FILE_NAME       "applog.txt"

#define CheckBit(x,y)   (x & y) == y

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
    LogVerbose_Info = 'I',          //Information
    LogVerbose_Warning = 'W',       //Warning message
    LogVerbose_Error = 'E',         //Error message
    LogVerbose_Command = 'C'        //Command
};

class CAppLogHandler : public QObject
{
    Q_OBJECT

public:
    CAppLogHandler();
    CAppLogHandler(char* logname, int logoption);
    ~CAppLogHandler();

    virtual void WriteLine(char* message, int verbose = LogVerbose_Info);
    virtual void WriteLine(const QString message, int verbose = LogVerbose_Info);
    void Write(char* message, int verbose);
    void SetNewLogFile(char* filepath, char* filename, int maxlogfilesize = DEFAULT_APP_LOG_FILE_SIZE);
    int GetLogOptions() { return m_log_options; }
    void SetLogOptions(int logoption);
    void SetLogName(char* logname) { m_log_name = logname;}

    template <typename T>
    CAppLogHandler& operator<<(const T& t) { QTextStream ts(&m_message); ts << t ; return *this; }  // default operator << function

    CAppLogHandler& operator<<(LogVerbose t);
    CAppLogHandler& operator<<(const char* t);
    CAppLogHandler& operator<<(const QString & t);
    CAppLogHandler& operator<<(std::ostream&(*t)(std::ostream&));

signals:
     void NewLogMessage(QString message, bool newline);

protected:
    std::ofstream flog;
    QString m_log_name;
    QString m_log_file_path;
    QString m_log_file_name;
    QString m_message;
    int m_cur_stream_output_verbose;    //verbose indicates message type when using "applog << message";

    int m_log_options;
    int m_max_log_file_size;
    QMutex m_file_lock;

    void AcquireLock();
    void ReleaseLock();
    void InitLogFile();
    QString GetCurrentAppDirectory();
    QString GetCurrentTimeString();
    QString GetCurrentTimeFileString();
};

#endif // CLASS_APPLOG_H
