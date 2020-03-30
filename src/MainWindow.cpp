#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ClassChip.h"
#include "ClassSim.h"
#include "ClassSimX.h"
#include "ClassWatch.h"
#include "CommandWindow.h"
#include "FormImageView.h"
#include "FormWaveView.h"
#include "LogWindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Create our list of nets to track
    m_watch = new ClassWatch(this);

    // Create interface to the simulation code
    m_sim = new ClassSim(this);

    // Create the main chip class
    m_chip = new ClassChip(this);

    // Create the chip netlist simulator code class
    m_simx = new ClassSimX(this);

    // Create a central widget to show a chip image
    setCentralWidget(new FormImageView(this, m_chip, m_simx));

    // Find various menu handles since we will be managing its objects dynamically
    m_menuView = menuBar()->findChild<QMenu *>("menuView");
    m_menuWindow = menuBar()->findChild<QMenu *>("menuWindow");
    menuBar()->setNativeMenuBar(false);

    // Enable all bells and whistles of the Qt docking engine!
    setDockOptions(AllowNestedDocks | AnimatedDocks | AllowTabbedDocks);

    // Create and dock the log window
    m_logWindow = new LogWindow(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_logWindow);
    QAction *actionLog = m_logWindow->toggleViewAction();
    actionLog->setShortcut(QKeySequence("F2"));
    actionLog->setStatusTip("Show or hide the application log window");
    m_menuView->addAction(actionLog);

    // Create and dock the cmd window
    m_cmdWindow = new CommandWindow(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_cmdWindow);
    QAction *actionCmd = m_cmdWindow->toggleViewAction();
    actionCmd->setShortcut(QKeySequence("F12"));
    actionCmd->setStatusTip("Show or hide the command window");
    m_menuView->addAction(actionCmd);

    // Let the log and command windows use the same space on the bottom
    tabifyDockWidget(m_logWindow, m_cmdWindow);
    m_logWindow->show();
    m_cmdWindow->hide();

    // Connect the rest of the menu actions...
    connect(ui->actionOpenChipDir, SIGNAL(triggered()), this, SLOT(onOpenChipDir()));
    connect(ui->actionReload, SIGNAL(triggered()), this, SLOT(loadResources()));
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(onExit()));
    connect(ui->actionNewImageView, SIGNAL(triggered()), this, SLOT(onNewImageView()));
    connect(ui->actionNewWaveformView, SIGNAL(triggered()), this, SLOT(onNewWaveformView()));
    connect(ui->actionLoadWatchlist, SIGNAL(triggered()), this, SLOT(onLoadWatchlist()));
    connect(ui->actionSaveWatchlistAs, SIGNAL(triggered()), this, SLOT(onSaveWatchlistAs()));
    connect(ui->actionSaveWatchlist, SIGNAL(triggered()), this, SLOT(onSaveWatchlist()));

    // As soon as the GUI becomes idle, load chip resources
    QTimer::singleShot(0, this, SLOT(loadResources()));
}

/*
 * Main window destructor. Do all clean-ups here.
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/*
 * Menu action to exit the application
 */
void MainWindow::onExit()
{
    close();
}

/*
 * Handle menu item to open directory with chip resources
 */
void MainWindow::onOpenChipDir()
{
    // Prompts the user to select the chip resource folder
    QString fileName = QFileDialog::getOpenFileName(this, "Select chip resource folder", "", "Images (*.png)");
    if (!fileName.isEmpty())
    {
        QString path = QFileInfo(fileName).path();
        if (m_chip->loadChipResources(path))
        {
            QSettings settings;
            settings.setValue("ChipResources", path);
        }
        else
            QMessageBox::critical(this, "Error", "Selected folder does not contain expected chip resources");
    }
}

/*
 * Loads application resources
 */
void MainWindow::loadResources()
{
    QSettings settings;

    // Load last recently used list of watches
    QString file = settings.value("WatchlistFile", "").toString();
    m_watch->loadWatchlist(file);

    QString path = settings.value("ChipResources", QDir::currentPath()).toString();
    while (!m_chip->loadChipResources(path))
    {
        // Prompts the user to select the chip resource folder
        QString fileName = QFileDialog::getOpenFileName(this, "Select chip resource folder", "", "Images (*.png)");
        if (!fileName.isEmpty())
            path = QFileInfo(fileName).path();
    }
    settings.setValue("ChipResources", path);

    while (!m_sim->loadSimResources(path))
    {
        // Prompts the user to select the chip resource folder
        QString fileName = QFileDialog::getOpenFileName(this, "Select netlist file", "", "Netlist (*.netlist)");
        if (!fileName.isEmpty())
            path = QFileInfo(fileName).path();
    }
    settings.setValue("ChipResources", path);

    if (!m_simx->loadResources(path))
    {
        // Prompts the user to select the chip resource folder
        QString fileName = QFileDialog::getOpenFileName(this, "Select simX resource folder", "", "Javascript (*.js)");
        if (!fileName.isEmpty())
            path = QFileInfo(fileName).path();
    }
    settings.setValue("ChipResources", path);
}

/*
 * Handle menu item to create a new image view
 */
void MainWindow::onNewImageView()
{
    QDockWidget *dock = new QDockWidget("Image View", this);
    FormImageView *w = new FormImageView(dock, m_chip, m_simx);
    dock->setWidget(w);

    addDockWidget(Qt::BottomDockWidgetArea, dock);
    dock->setFloating(true);
    dock->resize(300, 300);

    w->onRefresh();
    w->show();
}

/*
 * Handle menu item to create a new waveform window
 */
void MainWindow::onNewWaveformView()
{
    QDockWidget *dock = new QDockWidget("Waveform", this);
    FormWaveView *w = new FormWaveView(dock, m_sim);
    dock->setWidget(w);

    addDockWidget(Qt::BottomDockWidgetArea, dock);
    dock->setFloating(true);
    dock->resize(300, 300);

    //w->onRefresh();
    w->show();
}

void MainWindow::onLoadWatchlist()
{
    QSettings settings;
    QString defName = settings.value("WatchlistFile", "watchlist.wlist").toString();
    // Prompts the user to select which watchlist file to load
    QString fileName = QFileDialog::getOpenFileName(this, "Select watchlist file to load", defName, "watchlist (*.wlist)");
    if (!fileName.isEmpty())
    {
        if (m_watch->loadWatchlist(fileName))
        {
            QSettings settings;
            settings.setValue("WatchlistFile", fileName);
        }
        else
            QMessageBox::critical(this, "Error", "Selected file is not a valid watchlist file");
    }
}

void MainWindow::onSaveWatchlistAs()
{
    // Prompts the user to select the watches file to load
    QString fileName = QFileDialog::getSaveFileName(this, "Save watchlist file", "", "watchlist (*.wlist)");
    if (!fileName.isEmpty())
    {
        if (m_watch->saveWatchlist(fileName))
        {
            QSettings settings;
            settings.setValue("WatchlistFile", fileName);
        }
        else
            QMessageBox::critical(this, "Error", "Unable to save watchlist file " + fileName);
    }
}

void MainWindow::onSaveWatchlist()
{
    QSettings settings;
    QString fileName = settings.value("WatchlistFile", "watchlist.wlist").toString();
    if (!m_watch->saveWatchlist(fileName))
        QMessageBox::critical(this, "Error", "Unable to save watchlist file " + fileName);
}
