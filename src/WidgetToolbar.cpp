#include "WidgetToolbar.h"
#include "ui_WidgetToolbar.h"
#include "ClassController.h"

WidgetToolbar::WidgetToolbar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetToolbar)
{
    ui->setupUi(this);

    connect(ui->btRun, &QPushButton::clicked, &::controller, []() { emit ::controller.doRunsim(INT_MAX); });
    connect(ui->btStop, &QPushButton::clicked, &::controller, []() { emit ::controller.doRunsim(0); });
    connect(ui->btStep, &QPushButton::clicked, &::controller, [this]() { emit ::controller.doRunsim(ui->spinBox->value()); });
}

WidgetToolbar::~WidgetToolbar()
{
    delete ui;
}
