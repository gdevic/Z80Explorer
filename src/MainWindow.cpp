#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ClassController.h"
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
    m_logWindow = logWindow;
    addDockWidget(Qt::BottomDockWidgetArea, m_logWindow);
    QAction *actionLog = m_logWindow->toggleViewAction();
    actionLog->setShortcut(QKeySequence("F2"));
    actionLog->setStatusTip("Show or hide the application log window");
    m_menuView->addAction(actionLog);

    // Create and dock the cmd window
    m_cmdWindow = new DockCommand(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_cmdWindow);
    QAction *actionCmd = m_cmdWindow->toggleViewAction();
    actionCmd->setShortcut(QKeySequence("F12"));
    actionCmd->setStatusTip("Show or hide the command window");
    m_menuView->addAction(actionCmd);

    // Create and dock the monitor window
    m_monitor = new DockMonitor(this);
    addDockWidget(Qt::RightDockWidgetArea, m_monitor);

    // Let the log and command windows use the same space on the bottom
    tabifyDockWidget(m_logWindow, m_cmdWindow);
    m_logWindow->show();
    m_cmdWindow->hide();
    m_monitor->show();

    // Connect the rest of the menu actions...
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(onExit()));
    connect(ui->actionNewImageView, SIGNAL(triggered()), this, SLOT(onNewImageView()));
    connect(ui->actionNewWaveformView, SIGNAL(triggered()), this, SLOT(onNewWaveformView()));
    connect(ui->actionLoadWatchlist, SIGNAL(triggered()), this, SLOT(onLoadWatchlist()));
    connect(ui->actionSaveWatchlistAs, SIGNAL(triggered()), this, SLOT(onSaveWatchlistAs()));
    connect(ui->actionSaveWatchlist, SIGNAL(triggered()), this, SLOT(onSaveWatchlist()));
    connect(ui->actionEditWatchlist, SIGNAL(triggered()), this, SLOT(onEditWatchlist()));
}

/*
 * Main window destructor. Do all clean-ups here.
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
    DockImageView *w = new DockImageView(this, ++m_lastImageWndId);
    ui->menuWindow->addAction(w->toggleViewAction());
    w->setFloating(true);
    w->resize(800, 800);
    w->show();
}

/*
 * Handle menu item to create a new waveform window
 */
void MainWindow::onNewWaveformView()
{
    DockWaveform *w = new DockWaveform(this, ++m_lastWaveWndId);
    ui->menuWindow->addAction(w->toggleViewAction());
    w->setFloating(true);
    w->resize(1000, 500);
    w->show();
}

void MainWindow::onEditWatchlist()
{
    DialogEditWatchlist dlg(this);
    dlg.setNodeList(::controller.getNetlist().getNodenames());
    dlg.setWatchlist(::controller.getWatch().getWatchlist());
    if (dlg.exec()==QDialog::Accepted)
        ::controller.getWatch().updateWatchlist(dlg.getWatchlist());
}

void MainWindow::onLoadWatchlist()
{
    QSettings settings;
    QString defName = settings.value("WatchlistFile", "watchlist.wt").toString();
    // Prompts the user to select which watchlist file to load
    QString fileName = QFileDialog::getOpenFileName(this, "Select watchlist file to load", defName, "watchlist (*.wl);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (::controller.getWatch().loadWatchlist(fileName))
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
    QString fileName = QFileDialog::getSaveFileName(this, "Save watchlist file", "", "watchlist (*.wl);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (::controller.getWatch().saveWatchlist(fileName))
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
    QString fileName = settings.value("WatchlistFile", "watchlist.wl").toString();
    if (!::controller.getWatch().saveWatchlist(fileName))
        QMessageBox::critical(this, "Error", "Unable to save watchlist file " + fileName);
}
