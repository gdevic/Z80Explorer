#include "MainWindow.h"
#include "ClassApplog.h"
#include "ClassSingleton.h"

#include <QApplication>
#include <QMessageBox>
#include <QSettings>

// Global objects
MainWindow *mainWindow = 0; // Window: main window class
CAppLogHandler *applog = 0; // Application logging subsystem

/*
 * Handler for both Qt messages and application messages.
 * The output is forked to application logger and the log window
 */
void appLogMsgHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QSettings settings;
    int logLevel = settings.value("AppLogLevel", 2).toInt();

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

int main(int argc, char *argv[])
{
    int retCode = -1;
    QApplication a(argc, argv);

    // Wrap the application code with an exception handler
    try
    {
        // Initialize core application attributes
        QCoreApplication::setOrganizationDomain("BaltazarStudios.com");
        QCoreApplication::setOrganizationName("Baltazar Studios, LLC");
        QCoreApplication::setApplicationName("Z80qsim");

        // Initialize logging subsystem and register our handler
        applog = &Singleton<CAppLogHandler>::Instance();
        char logName[] = "z80qsim";
        applog->SetLogName(logName);
        applog->SetLogOptions(applog->GetLogOptions() | LogOptions_Signal);

        // Install the message hook into the log window so we can use qDebug, etc.
        qInstallMessageHandler(appLogMsgHandler);

        // Create main application window
        mainWindow = new MainWindow();

        // Main initialization outside the constructor
        while (mainWindow->init())
        {
            QSettings settings;

            // Load and set main window location and size
            // Include also all docking windows location, size and docking status
            mainWindow->restoreGeometry(settings.value("MainWindow/Geometry").toByteArray());
            mainWindow->restoreState(settings.value("MainWindow/State").toByteArray());

            // Show the main window
            mainWindow->show();

            // Run the application main code loop
            retCode = a.exec();

            // Save window configuration after the main application finished executing
            // Include also all docking windows location, size and docking status
            settings.setValue("MainWindow/State", mainWindow->saveState());
            settings.setValue("MainWindow/Geometry", mainWindow->saveGeometry());

            break;
        }
        delete mainWindow;
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
