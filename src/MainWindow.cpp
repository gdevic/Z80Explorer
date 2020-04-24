#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ClassController.h"
#include "DialogEditBuses.h"
#include "DialogEditWatchlist.h"
#include "DockWaveform.h"
#include "DockCommand.h"
#include "DockImageView.h"
#include "DockMonitor.h"
#include "DockLog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent, DockLog *logWindow) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialize image view that is embedded within the central pane
    ui->widgetImageView->init();

    // Find various menu handles since we will be managing its objects dynamically
    m_menuView = menuBar()->findChild<QMenu *>("menuView");
    m_menuWindow = menuBar()->findChild<QMenu *>("menuWindow");
    menuBar()->setNativeMenuBar(false);

    // Enable all bells and whistles of the Qt docking engine!
    setDockOptions(AllowNestedDocks | AnimatedDocks | AllowTabbedDocks);

    // Log window has been created by the main.cpp and here we simply re-parent it
    m_log = logWindow;
    addDockWidget(Qt::BottomDockWidgetArea, m_log);
    QAction *actionLog = m_log->toggleViewAction();
    actionLog->setShortcut(QKeySequence("F2"));
    actionLog->setStatusTip("Show or hide the application log window");
    m_menuView->addAction(actionLog);

    // Create and dock the cmd window
    m_cmd = new DockCommand(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_cmd);
    QAction *actionCmd = m_cmd->toggleViewAction();
    actionCmd->setShortcut(QKeySequence("F12"));
    actionCmd->setStatusTip("Show or hide the command window");
    m_menuView->addAction(actionCmd);

    // Create and dock the monitor window
    m_monitor = new DockMonitor(this);
    addDockWidget(Qt::RightDockWidgetArea, m_monitor);
    QAction *actionMonitor = m_monitor->toggleViewAction();
    actionMonitor->setShortcut(QKeySequence("F11"));
    actionMonitor->setStatusTip("Show or hide the monitor window");
    m_menuView->addAction(actionMonitor);

    // Load and set main window location and size
    // Include also all docking windows location, size and docking status
    QSettings settings;
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

    // Connect the rest of the menu actions...
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(onExit()));
    connect(ui->actionNewImageView, SIGNAL(triggered()), this, SLOT(onNewImageView()));
    connect(ui->actionNewWaveformView, SIGNAL(triggered()), this, SLOT(onNewWaveformView()));
    connect(ui->actionLoadWatchlist, SIGNAL(triggered()), this, SLOT(onLoadWatchlist()));
    connect(ui->actionSaveWatchlistAs, SIGNAL(triggered()), this, SLOT(onSaveWatchlistAs()));
    connect(ui->actionSaveWatchlist, SIGNAL(triggered()), this, SLOT(onSaveWatchlist()));
    connect(ui->actionEditBuses, SIGNAL(triggered()), this, SLOT(onEditBuses()));
    connect(ui->actionEditWatchlist, SIGNAL(triggered()), this, SLOT(onEditWatchlist()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(onAbout()));

    // Do the initial simulated chip reset so that we wake up the app in some useful state
    ::controller.doReset();
}

/*
 * Main window destructor.
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/*
 * Called on application close event
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    // Save window configuration after the main application finished executing
    // Include also all docking windows location, size and docking status
    QSettings settings;
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());

    onSaveWatchlist();
    event->accept();
}

/*
 * Menu action to exit the application
 */
void MainWindow::onExit()
{
    close();
}

/*
 * Handle menu item to create a new image view
 */
void MainWindow::onNewImageView()
{
    if (m_lastImageWndId < 4)
    {
        DockImageView *w = new DockImageView(this, ++m_lastImageWndId);
        ui->menuWindow->addAction(w->toggleViewAction());
        w->show();
    }
    else
        QMessageBox::critical(this, "New Image View", "You can create up to 4 image views. Please switch to one of those that are already created.");
}

/*
 * Handle menu item to create a new waveform window
 */
void MainWindow::onNewWaveformView()
{
    if (m_lastWaveWndId < 4)
    {
        DockWaveform *w = new DockWaveform(this, ++m_lastWaveWndId);
        ui->menuWindow->addAction(w->toggleViewAction());
        w->show();
    }
    else
        QMessageBox::critical(this, "New Waveform", "You can create up to 4 waveform views. Please switch to one of those that are already created.");
}

/*
 * Handle menu item to edit nets and buses
 */
void MainWindow::onEditBuses()
{
    DialogEditBuses dlg(this);
    dlg.exec();
}

void MainWindow::onEditWatchlist()
{
    DialogEditWatchlist dlg(this);
    dlg.setNodeList(::controller.getNetlist().getNetnames());
    dlg.setWatchlist(::controller.getWatch().getWatchlist());
    if (dlg.exec()==QDialog::Accepted)
        ::controller.getWatch().updateWatchlist(dlg.getWatchlist());
}

void MainWindow::onLoadWatchlist()
{
    QSettings settings;
    QString fileWatchlist = settings.value("WatchlistFile").toString();
    Q_ASSERT(!fileWatchlist.isEmpty());
    // Prompts the user to select which watchlist file to load
    QString fileName = QFileDialog::getOpenFileName(this, "Select watchlist file to load", fileWatchlist, "watchlist (*.wl);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (::controller.getWatch().loadWatchlist(fileName))
        {
            QSettings settings; // Store the file name to be used next time as default
            settings.setValue("WatchlistFile", fileName);
        }
        else
            QMessageBox::critical(this, "Error", "Selected file is not a valid watchlist file");
    }
}

void MainWindow::onSaveWatchlistAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save watchlist file", "", "watchlist (*.wl);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (::controller.getWatch().saveWatchlist(fileName))
        {
            QSettings settings; // Store the file name to be used next time as default
            settings.setValue("WatchlistFile", fileName);
        }
        else
            QMessageBox::critical(this, "Error", "Unable to save watchlist file " + fileName);
    }
}

void MainWindow::onSaveWatchlist()
{
    QSettings settings;
    QString fileName = settings.value("WatchlistFile").toString();
    Q_ASSERT(!fileName.isEmpty());
    if (!::controller.getWatch().saveWatchlist(fileName))
        QMessageBox::critical(this, "Error", "Unable to save watchlist file " + fileName);
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "Z80 Explorer", "<a href='https://baltazarstudios.com'>https://baltazarstudios.com</a><br>by Goran Devic");
}
