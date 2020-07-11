#include "DialogEditNets.h"
#include "ui_DialogEditNets.h"

DialogEditNets::DialogEditNets(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditNets)
{
    ui->setupUi(this);
}

DialogEditNets::~DialogEditNets()
{
    delete ui;
}
