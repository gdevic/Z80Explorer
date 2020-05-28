#include "DockMonitor.h"
#include "ui_DockMonitor.h"
#include "ClassController.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStringBuilder>

DockMonitor::DockMonitor(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockMonitor)
{
    ui->setupUi(this);

    connect(&::controller.getTrickbox(), SIGNAL(echo(char)), this, SLOT(onEcho(char)));
    connect(&::controller.getTrickbox(), SIGNAL(echo(QString)), this, SLOT(onEcho(QString)));
    connect(&::controller.getTrickbox(), SIGNAL(refresh()), this, SLOT(refresh()));
    connect(&::controller, &ClassController::onRunStopped, this, &DockMonitor::refresh);
    connect(ui->btLoad, &QPushButton::clicked, this, &DockMonitor::onLoad);
    connect(ui->btReload, &QPushButton::clicked, this, &DockMonitor::onReload);
}

DockMonitor::~DockMonitor()
{
    delete ui;
}

void DockMonitor::onLoad()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Select Intel HEX file to load into simulated RAM", "", "File (*.hex);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!::controller.loadHex(fileName))
            QMessageBox::critical(this, "Error", "Error loading " + fileName);
    }
}

void DockMonitor::onReload()
{
    if (!::controller.loadHex(QString()))
        QMessageBox::critical(this, "Error", "Error loading HEX file");
}

/*
 * Write out a character to the virtual console
 */
void DockMonitor::onEcho(char c)
{
    ui->textTerminal->moveCursor (QTextCursor::End);
    ui->textTerminal->insertPlainText(QChar(c));
    ui->textTerminal->moveCursor (QTextCursor::End);
}

/*
 * Write out a string to the virtual console
 */
void DockMonitor::onEcho(QString s)
{
    ui->textTerminal->moveCursor (QTextCursor::End);
    ui->textTerminal->insertPlainText(s);
    ui->textTerminal->moveCursor (QTextCursor::End);
}

/*
 * Refresh the monitor information box
 * Also, called by the sim when the current run stops at a given half-cycle
 */
void DockMonitor::refresh()
{
    static z80state z80;
    ::controller.readState(z80);
    const QString monitor = ::controller.readMonitor();
    ui->textStatus->setPlainText(z80state::dumpState(z80) % monitor);
}
