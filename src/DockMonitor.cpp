#include "DockMonitor.h"
#include "ui_DockMonitor.h"
#include "ClassController.h"
#include <QFileDialog>
#include <QMessageBox>

DockMonitor::DockMonitor(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockMonitor)
{
    ui->setupUi(this);

    connect(&::controller, SIGNAL(echo(char)), this, SLOT(onEcho(char)));
}

DockMonitor::~DockMonitor()
{
    delete ui;
}

void DockMonitor::on_btLoadHex_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Select Intel HEX file to load into RAM", "", "File (*.hex);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!::controller.loadIntelHex(fileName))
            QMessageBox::critical(this, "Error", "Error loading" + fileName);
    }
}

void DockMonitor::onEcho(char c)
{
    QString s = ui->textTerminal->toPlainText();
    s.append(QChar(c));
    ui->textTerminal->setPlainText(s);
}
