#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ClassDockCollection.h"
#include "CommandWindow.h"
#include "LogWindow.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_menuView(0),
    m_logWindow(0),
    m_cmdWindow(0),
    m_docks(0)
{
    ui->setupUi(this);

    // Find various menu handles since we will be managing its objects dynamically
    m_menuView = menuBar()->findChild<QMenu *>("menuView");
    m_menuWindow = menuBar()->findChild<QMenu *>("menuWindow");
    menuBar()->setNativeMenuBar(false);

    // Create the dock window collection class that keeps docking windows in check
    m_docks = new ClassDockCollection(this);

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
    m_cmdWindow->show();

    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(onExit()));
}

/**
 * Main window destructor. Do all clean-ups here.
 */
MainWindow::~MainWindow()
{
    delete m_cmdWindow;
    delete m_logWindow;
    delete ui;
}

/**
 * This initialization is done after the window construction. The main benefit is
 * that it can fail (return false) and thus shut down the application.
 */
bool MainWindow::init()
{
    qDebug() << "Main init";
    return true;
}

/*
 * Menu action to exit the application
 */
void MainWindow::onExit()
{
    close();
}
