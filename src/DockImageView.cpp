#include "DockImageView.h"
#include "ui_DockImageView.h"
#include "ClassChip.h"
#include "ClassController.h"

DockImageView::DockImageView(QWidget *parent, uint id) :
    QDockWidget(parent),
    ui(new Ui::DockImageView)
{
    ui->setupUi(this);
    setWindowTitle("Image View " + QString::number(id));
    ui->pane->init();
}

DockImageView::~DockImageView()
{
    delete ui;
}
