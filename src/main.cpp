#include "MainWindow.h"

#include <QApplication>
#include <QMessageBox>

// Global objects
MainWindow *mainWindow = 0;         // Window: main window class

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
