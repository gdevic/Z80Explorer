#include "WidgetToolbar.h"
#include "ui_WidgetToolbar.h"
#include "ClassController.h"
#include <QStringBuilder>
#include <QDebug>

WidgetToolbar::WidgetToolbar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetToolbar)
{
    ui->setupUi(this);

    connect(&::controller, SIGNAL(onRunStopped(uint)), this, SLOT(onRunStopped(uint)));

    connect(ui->btRun, &QPushButton::clicked, &::controller, []() { emit ::controller.doRunsim(INT_MAX); });
    connect(ui->btStop, &QPushButton::clicked, &::controller, []() { emit ::controller.doRunsim(0); });
    connect(ui->btStep, &QPushButton::clicked, &::controller, [this]() { emit ::controller.doRunsim(ui->spinStep->value()); });
    connect(ui->btReset, &QPushButton::clicked, &::controller, []() { emit ::controller.doReset(); });
    connect(ui->btRestart, &QPushButton::clicked, this, &WidgetToolbar::doRestart);
}

WidgetToolbar::~WidgetToolbar()
{
    delete ui;
}

void WidgetToolbar::onRunStopped(uint hcycle)
{
    ui->labelCycle->setText("hcycle: " % QString::number(hcycle));
}

/*
 * Restart simulation and stop at the clock number set in the spinbox
 */
void WidgetToolbar::doRestart()
{
    qInfo() << "Restarting simulation up to hclock " << ui->spinRestart->value();
    uint hcycle = ::controller.doReset();
    ::controller.doRunsim(ui->spinRestart->value() - hcycle);
}
