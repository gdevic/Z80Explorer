#include "MainWindow.h"
#include "ClassApplog.h"
#include "ClassController.h"
#include "ClassSingleton.h"
#include "DockLog.h"
#include <signal.h>
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QVBoxLayout>

// Global objects
MainWindow *mainWindow = nullptr; // Window: main window class
CAppLogHandler *applog = nullptr; // Application logging subsystem
ClassController controller {}; // Application-wide controller class

static void crashMessage(std::string e = "Oh, no!")
{
    QMessageBox::critical(nullptr, "App crashed", QString::fromStdString(e) + "\n"
                          "Application has unexpectedly terminated. Please report this as a bug. "
                          "If you can also reproduce the issue, please provide the steps. Thank you!");
    qFatal(e.c_str());
}

int main(int argc, char *argv[])
{
    int retCode = -1;
    // Handle faults that try..catch can't capture, by a low-level signal handler
    signal(SIGSEGV, [](int signum) { ::signal(signum, SIG_DFL); crashMessage(); });
    QApplication a(argc, argv);
    QScriptEngine scriptEngine;

    // Wrap the application code with an exception handler in release build
    // In debug build we want to catch problems within the running debugger
#ifdef QT_NO_DEBUG
    try
#endif // QT_NO_DEBUG
    {
        // Initialize core application attributes
        QCoreApplication::setOrganizationDomain("BaltazarStudios.com");
        QCoreApplication::setOrganizationName("Baltazar Studios, LLC");
        QCoreApplication::setApplicationName("Z80Explorer");
        QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);

        // Initialize logging subsystem and register our handler
        applog = &Singleton<CAppLogHandler>::Instance();
        applog->SetLogOptions(applog->GetLogOptions() | LogOptions_Signal /* | LogOptions_File */);

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
            mainWindow = new MainWindow(nullptr, logWindow, &scriptEngine);
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
#ifdef QT_NO_DEBUG
    catch(std::exception& e)
    {
        crashMessage(e.what());
    }
    catch(...)
    {
        crashMessage();
    }
#endif // QT_NO_DEBUG

    return retCode;
}
