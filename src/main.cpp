#include "MainWindow.h"
#include "ClassApplog.h"
#include "ClassController.h"
#include "ClassSingleton.h"
#include "DockLog.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QSettings>
#include <QVBoxLayout>
#include <QtScript>

// Global objects
MainWindow *mainWindow = nullptr; // Window: main window class
CAppLogHandler *applog = nullptr; // Application logging subsystem
ClassController controller {}; // Application-wide controller class

/*
 * Handler for both Qt messages and application messages.
 * The output is forked to application logger and the log window
 */
void appLogMsgHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QSettings settings;
    // XXX This is temporary: logLevel should be 2 (not to include qDebug() messages)
    int logLevel = settings.value("AppLogLevel", 3).toInt();

    QByteArray localMsg = msg.toLocal8Bit();
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

//----------------------------------------------------------------------------
// Support for Z80_Simulator code
//----------------------------------------------------------------------------
void yield()
{
    QEventLoop e; // Don't freeze the GUI
    e.processEvents(QEventLoop::AllEvents);
}

#include <stdarg.h>

static char log_buffer[1024]; // XXX UNSAFE !!
static int log_i = 0;

void logf(char *fmt, ...)
{
    va_list valist;
    va_start(valist, fmt);
    int i = vsprintf(log_buffer + log_i, fmt, valist);
    bool nl = false;
    if (i > 0)
    {
        if (log_i + i >= 900) // "Safety"
            nl = true;
        if (log_buffer[log_i + i - 1] == '\n')
        {
            nl = true;
            log_buffer[log_i + i - 1] = 0;
        }
    }
    if (nl)
    {
        log_i = 0;
        QString s = QString::fromUtf8(log_buffer);
        qInfo() << s;
    }
    else
        log_i = log_i + i;
    yield();
}
//----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    int retCode = -1;
    QApplication a(argc, argv);
    QScriptEngine scriptEngine;

    // Wrap the application code with an exception handler
    try
    {
        // Initialize core application attributes
        QCoreApplication::setOrganizationDomain("BaltazarStudios.com");
        QCoreApplication::setOrganizationName("Baltazar Studios, LLC");
        QCoreApplication::setApplicationName("Z80Explorer");

        // Initialize logging subsystem and register our handler
        applog = &Singleton<CAppLogHandler>::Instance();
        char logName[] = "z80explorer";
        applog->SetLogName(logName);
        applog->SetLogOptions(applog->GetLogOptions() | LogOptions_Signal);

        // Install the message hook into the log window so we can use qDebug, etc.
        qInstallMessageHandler(appLogMsgHandler);

        // Create a temporary window that contains the log widget so we can observe init log messages
        QWidget *wndInit = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(wndInit);
        DockLog *logWindow = new DockLog(wndInit);
        layout->addWidget(logWindow);
        wndInit->setWindowState(Qt::WindowMaximized);
        wndInit->show();

        // Initialize the controller object outside the constructor
        if (::controller.init(&scriptEngine))
        {
            wndInit->hide(); // Hide the initialization log window

            // Create, show and run main application window
            mainWindow = new MainWindow(nullptr, logWindow);
            mainWindow->show();
            retCode = a.exec();

            delete mainWindow;
        }
        else
        {
            QMessageBox::critical(0, "Error", "Error initializing the application!");
        }
        delete wndInit;
    }
    catch(std::exception& e)
    {
        QString s = QString::fromStdString(e.what()) + "\nPlease report this incident as a bug!";
        QMessageBox::critical(0, "Application exception", s);
    }
    catch(...)
    {
        QMessageBox::critical(0, "Application exception", "Unexpected error. Please report this incident as a bug!");
    }

    return retCode;
}
