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
    connect(&::controller, &ClassController::onRunStarting, this, [this]() { m_timer.start(500); });

    connect(ui->btRun, &QPushButton::clicked, &::controller, []() { ::controller.doRunsim(INT_MAX); });
    connect(ui->btStop, &QPushButton::clicked, &::controller, []() { ::controller.doRunsim(0); });
    connect(ui->btStep, &QPushButton::clicked, &::controller, [this]() { ::controller.doRunsim(ui->spinStep->value()); });
    connect(ui->btReset, &QPushButton::clicked, &::controller, []() { ::controller.doReset(); });
    connect(ui->btRestart, &QPushButton::clicked, this, &WidgetToolbar::doRestart);

    connect(&m_timer, &QTimer::timeout, this, &WidgetToolbar::onTimeout);
}

WidgetToolbar::~WidgetToolbar()
{
    delete ui;
}

void WidgetToolbar::onTimeout()
{
    ui->btRun->setText("Running...");
    ui->btRun->setStyleSheet((++m_blinkPhase & 1) ? "background-color: lightgreen" : "");
    uint hcycle = ::controller.getSimZ80().getCurrentHCycle();
    ui->labelCycle->setText("hcycle: " % QString::number(hcycle));
}

void WidgetToolbar::onRunStopped(uint hcycle)
{
    m_timer.stop();
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
