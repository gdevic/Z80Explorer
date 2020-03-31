#include "DockImageView.h"
#include "ui_DockImageView.h"

#include <ClassChip.h>
#include <ClassSimX.h>

DockImageView::DockImageView(QWidget *parent, ClassChip *chip, ClassSimX *simx) :
    QDockWidget(parent),
    ui(new Ui::DockImageView)
{
    ui->setupUi(this);
    ui->pane->init(chip, simx);
}

DockImageView::~DockImageView()
{
    delete ui;
}
