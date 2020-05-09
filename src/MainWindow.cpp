#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ClassController.h"
#include "DialogEditAnnotations.h"
#include "DialogEditBuses.h"
#include "DialogEditWatchlist.h"
#include "DockWaveform.h"
#include "DockCommand.h"
#include "DockImageView.h"
#include "DockMonitor.h"
#include "DockLog.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

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
    actionLog->setShortcut(QKeySequence("F10"));
    actionLog->setStatusTip("Show or hide the application log window");
    m_menuView->addAction(actionLog);

    // Create and dock the cmd window
    m_cmd = new DockCommand(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_cmd);
    QAction *actionCmd = m_cmd->toggleViewAction();
    actionCmd->setShortcut(QKeySequence("F11"));
    actionCmd->setStatusTip("Show or hide the command window");
    m_menuView->addAction(actionCmd);

    // Create and dock the monitor window
    m_monitor = new DockMonitor(this);
    addDockWidget(Qt::RightDockWidgetArea, m_monitor);
    QAction *actionMonitor = m_monitor->toggleViewAction();
    actionMonitor->setShortcut(QKeySequence("F12"));
    actionMonitor->setStatusTip("Show or hide the monitor window");
    m_menuView->addAction(actionMonitor);

    // Load and set main window location and size
    // Include also all docking windows location, size and docking status
    QSettings settings;
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

    // Connect the rest of the menu actions...
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(onExit()));
    connect(ui->actionEditAnnotations, SIGNAL(triggered()), this, SLOT(onEditAnnotations()));
    connect(ui->actionEditBuses, SIGNAL(triggered()), this, SLOT(onEditBuses()));
    connect(ui->actionEditWatchlist, SIGNAL(triggered()), this, SLOT(onEditWatchlist()));
    connect(ui->actionNewImageView, SIGNAL(triggered()), this, SLOT(onNewImageView()));
    connect(ui->actionNewWaveformView, SIGNAL(triggered()), this, SLOT(onNewWaveformView()));
    connect(ui->actionOnlineManual, SIGNAL(triggered()), this, SLOT(onOnlineManual()));
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
    qInfo() << "App shutdown...";
    emit ::controller.shutdown();

    // Save window configuration after the main application finished executing
    // Include also all docking windows location, size and docking status
    QSettings settings;
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());

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
 * Handle menu item to edit custom annotations
 */
void MainWindow::onEditAnnotations()
{
    DialogEditAnnotations dlg(this);
    dlg.exec();
}

/*
 * Handle menu item to edit nets and buses
 */
void MainWindow::onEditBuses()
{
    DialogEditBuses dlg(this);
    dlg.exec();
}

/*
 * Handle menu item to edit watchlist
 */
void MainWindow::onEditWatchlist()
{
    DialogEditWatchlist dlg(this);
    dlg.setNodeList(::controller.getNetlist().getNetnames());
    dlg.setWatchlist(::controller.getWatch().getWatchlist());
    if (dlg.exec()==QDialog::Accepted)
        ::controller.getWatch().updateWatchlist(dlg.getWatchlist());
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

void MainWindow::onOnlineManual()
{
    QString link = "https://baltazarstudios.com/z80explorer";
    QDesktopServices::openUrl(QUrl(link));
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "Z80 Explorer",
    "Version " + QString::number(APP_VERSION / 10) + "." + QString::number(APP_VERSION % 10) +
    "<br>Goran Devic<br><a href='https://baltazarstudios.com'>https://baltazarstudios.com</a><br>");
}
