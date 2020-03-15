#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(onExit()));
}

/**
 * Main window destructor. Do all clean-ups here.
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * This initialization is done after the window construction. The main benefit is
 * that it can fail (return false) and thus shut down the application.
 */
bool MainWindow::init()
{
    QSettings settings;

    qDebug() << "Main init";

    return true;
}

// Menu action to exit the application
void MainWindow::onExit()
{
    close();
}
