#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ClassChip.h"
#include "CommandWindow.h"
#include "FormImageView.h"
#include "LogWindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_menuView(0),
    m_logWindow(0),
    m_cmdWindow(0),
    m_chip(0)
{
    ui->setupUi(this);

    // Create the main chip class
    m_chip = new ClassChip();

    // Create a central widget to show a chip image
    setCentralWidget(new FormImageView(this, m_chip));

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
    m_cmdWindow->show();

    // Connect the rest of the menu actions...
    connect(ui->actionOpenChipDir, SIGNAL(triggered()), this, SLOT(onOpenChipDir()));
    connect(ui->actionReload, SIGNAL(triggered()), this, SLOT(onReload()));
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(onExit()));
    connect(ui->actionNewImageView, SIGNAL(triggered()), this, SLOT(onNewImageView()));

    // As soon as the GUI becomes idle, load chip resources
    QTimer::singleShot(0, this, SLOT(loadChipResources()));
}

/*
 * Main window destructor. Do all clean-ups here.
 */
MainWindow::~MainWindow()
{
    delete m_chip;
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
 * Load chip resources on startup
 */
void MainWindow::loadChipResources()
{
    QSettings settings;
    QString path = settings.value("ChipResources", QDir::currentPath()).toString();
    if (!m_chip->loadChipResources(path))
        onOpenChipDir(); // Make the user select the chip resource folder
}

/*
 * Handle menu item to create a new image view
 */
void MainWindow::onNewImageView()
{
    QDockWidget *dock = new QDockWidget("Image View", this);
    FormImageView *w = new FormImageView(dock, m_chip);
    w->show();
    dock->setWidget(w);

    addDockWidget(Qt::BottomDockWidgetArea, dock);
    dock->setFloating(true);
    dock->resize(300, 300);
}

/*
 * Handle menu item to reload chip data and reset all images
 */
void MainWindow::onReload()
{
    loadChipResources();
}
