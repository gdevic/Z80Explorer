#include "DialogSchematic.h"
#include "ui_DialogSchematic.h"

DialogSchematic::DialogSchematic(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogSchematic)
{
    ui->setupUi(this);
}

DialogSchematic::~DialogSchematic()
{
    delete ui;
}
