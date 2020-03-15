#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(onExit()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Menu action to exit the application
void MainWindow::onExit()
{
    close();
}
