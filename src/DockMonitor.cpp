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

    connect(&::controller, SIGNAL(echo(char)), this, SLOT(onEcho(char)));
    connect(&::controller, SIGNAL(onRunStopped(uint)), this, SLOT(onRunStopped(uint)));
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
        if (!::controller.loadIntelHex(fileName))
            QMessageBox::critical(this, "Error", "Error loading" + fileName);
        else
            m_lastLoadedHex = fileName;
    }
}

void DockMonitor::onReload()
{
    if (!m_lastLoadedHex.isEmpty())
    {
        if (!::controller.loadIntelHex(m_lastLoadedHex))
            QMessageBox::critical(this, "Error", "Error loading" + m_lastLoadedHex);
    }
}

/*
 * Controller signals us that a new character is ready to print into the virtual terminal
 */
void DockMonitor::onEcho(char c)
{
    ui->textTerminal->moveCursor (QTextCursor::End);
    ui->textTerminal->insertPlainText(QChar(c));
    ui->textTerminal->moveCursor (QTextCursor::End);
}

/*
 * Controller signals us that the current simulation run completed
 */
void DockMonitor::onRunStopped(uint hcycle)
{
    z80state z80; // Get and display the chip state
    ::controller.readState(z80);
    QString s = z80state::dumpState(z80);
    s += QString("\nCurrent hcycle:%1\n").arg(hcycle);
    ui->textStatus->setPlainText(s);
}
