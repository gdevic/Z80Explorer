#include "DialogEditColors.h"
#include "ui_DialogEditColors.h"

DialogEditColors::DialogEditColors(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditColors)
{
    ui->setupUi(this);
}

DialogEditColors::~DialogEditColors()
{
    delete ui;
}
