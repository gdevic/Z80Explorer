#include "WidgetToolbar.h"
#include "ui_WidgetToolbar.h"

WidgetToolbar::WidgetToolbar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetToolbar)
{
    ui->setupUi(this);
}

WidgetToolbar::~WidgetToolbar()
{
    delete ui;
}
