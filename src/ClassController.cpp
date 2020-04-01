#include "ClassController.h"

#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QSettings>

bool ClassController::init()
{
    QSettings settings;
    QString path = settings.value("ChipResources", QDir::currentPath()).toString();

    // Check if the current resource path contains some of the resources
    while (!QFile::exists(path + "/nodenames.js"))
    {
        // Prompts the user to select the chip resource folder
        QString fileName = QFileDialog::getOpenFileName(nullptr, "Select the application resource folder", "", "Any file (*.*)");
        if (!fileName.isEmpty())
            path = QFileInfo(fileName).path();
        else
            return false;
    }
    settings.setValue("ChipResources", path);

    // Since the init phase can take some time (loading images, processing them etc.)
    // show a popup to let the user know what is going on
    QLabel qPopup("Loading application resource files... Please wait...", nullptr, Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    QPalette qPalette = qPopup.palette();
    qPalette.setBrush(QPalette::Background, QColor(0xff, 0xe0, 0xc0));
    qPopup.setPalette(qPalette);
    qPopup.setFrameStyle(QLabel::Raised | QLabel::Panel);
    qPopup.setAlignment(Qt::AlignCenter);
    qPopup.setFixedSize(320, 200);
    qPopup.show();

    // Initialize all global classes using the given path to resource
    if (!m_chip.loadChipResources(path) || !m_sim.loadSimResources(path) || !m_simx.loadResources(path))
    {
        qCritical() << "Unable to load chip resources from" << path;
        return false;
    }

    // Load the optional watchlist file
    QString fileWatchlist = settings.value("WatchlistFile", "").toString();
    m_watch.loadWatchlist(fileWatchlist);

    qPopup.hide();

    return true;
}

/*
 * Controls the simulation
 */
void ClassController::doRunsim(uint ticks)
{
    emit m_simx.doRunsim(ticks);
}
