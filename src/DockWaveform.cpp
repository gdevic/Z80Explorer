#include "DockWaveform.h"
#include "ui_DockWaveform.h"

DockWaveform::DockWaveform(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockWaveform)
{
    ui->setupUi(this);
}

DockWaveform::~DockWaveform()
{
    delete ui;
}
