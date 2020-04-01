#include "WidgetToolbar.h"
#include "ui_WidgetToolbar.h"

WidgetToolbar::WidgetToolbar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetToolbar)
{
    ui->setupUi(this);

    connect(ui->btRun, &QPushButton::clicked, this, &WidgetToolbar::onRun);
    connect(ui->btStop, &QPushButton::clicked, this, &WidgetToolbar::onStop);
    connect(ui->btStep, &QPushButton::clicked, this, &WidgetToolbar::onStep);
}

WidgetToolbar::~WidgetToolbar()
{
    delete ui;
}

void WidgetToolbar::onRun()
{
}

void WidgetToolbar::onStop()
{
}

void WidgetToolbar::onStep()
{
}
