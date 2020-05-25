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

    connect(ui->btRun, &QPushButton::clicked, &::controller, [this]() { m_timer.start(500); ::controller.doRunsim(INT_MAX); });
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
    static uint phase = 0;
    ui->btRun->setStyleSheet((++phase & 1) ? "background-color: lightgreen" : "");
    ui->btRun->setText("Running...");
}

void WidgetToolbar::onRunStopped(uint hcycle)
{
    ui->labelCycle->setText("hcycle: " % QString::number(hcycle));
    m_timer.stop();
    ui->btRun->setText("Run");
    ui->btRun->setStyleSheet("");
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
