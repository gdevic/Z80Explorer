#include "DockImageView.h"
#include "ui_DockImageView.h"
#include "ClassChip.h"
#include "ClassController.h"

DockImageView::DockImageView(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockImageView)
{
    ui->setupUi(this);
    ui->pane->init();
}

DockImageView::~DockImageView()
{
    delete ui;
}
