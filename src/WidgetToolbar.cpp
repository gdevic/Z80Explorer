#include "ClassController.h"
#include "WidgetToolbar.h"
#include "ui_WidgetToolbar.h"
#include <QStringBuilder>

WidgetToolbar::WidgetToolbar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetToolbar)
{
    ui->setupUi(this);

    connect(&::controller, &ClassController::onRunStopped, this, &WidgetToolbar::onRunStopped);
    connect(&::controller, &ClassController::onRunStarting, this, [this]() { ui->btRun->setText("Running..."); });
    connect(&::controller, &ClassController::onRunHeartbeat, this, &WidgetToolbar::onHeartbeat);

    connect(ui->btRun, &QPushButton::clicked, &::controller, []() { ::controller.doRunsim(INT_MAX); });
    connect(ui->btStop, &QPushButton::clicked, &::controller, []() { ::controller.doRunsim(0); });
    //connect(ui->btStop, &QPushButton::clicked, &::controller.getScript(), &ClassScript::stop); // XXX Need to find a way to stop JavaScript engine
    connect(ui->btStep, &QPushButton::clicked, &::controller, [this]() { ::controller.doRunsim(ui->spinStep->value()); });
    connect(ui->btReset, &QPushButton::clicked, &::controller, []() { ::controller.doReset(); });
    connect(ui->btRestart, &QPushButton::clicked, this, &WidgetToolbar::doRestart);
}

WidgetToolbar::~WidgetToolbar()
{
    delete ui;
}

void WidgetToolbar::onHeartbeat(uint hcycle)
{
    ui->btRun->setStyleSheet(ui->btRun->styleSheet().isEmpty() ? "background-color: lightgreen" : "");

    ui->labelCycle->setText("hcycle: " % QString::number(hcycle));

    uint hz = ::controller.getSimZ80().getEstHz();
    ui->labelFreq->setText(QString::number(hz) % " Hz");
}

void WidgetToolbar::onRunStopped(uint hcycle)
{
    ui->btRun->setText("Run");
    ui->btRun->setStyleSheet("");
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
