#include "AppTypes.h"
#include "ClassController.h"
#include "DialogSettings.h"
#include "ui_DialogSettings.h"
#include <QCheckBox>
#include <QSettings>
#include <QSpinBox>

DialogSettings::DialogSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogSettings)
{
    ui->setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("editSettingsGeometry").toByteArray());

    // Populate widgets from current QSettings values (or factory defaults from AppTypes.h)
    ui->spinHistory->setValue(settings.value("historyDepth", HISTORY_DEPTH).toInt());
    ui->checkSocket->setChecked(settings.value("socketServer", SOCKET_SERVER).toBool());
    ui->spinSocketPort->setValue(settings.value("socketPort", SOCKET_PORT).toInt());
    ui->checkMcp->setChecked(settings.value("mcpServer", MCP_SERVER).toBool());
    ui->spinMcpPort->setValue(settings.value("mcpPort", MCP_PORT).toInt());

    // Lock the port field while the corresponding server is enabled — you cannot change a port
    // on a running server. To pick a new port: uncheck Enable, edit the port, re-check Enable.
    connect(ui->checkSocket, &QCheckBox::toggled, ui->spinSocketPort, &QWidget::setDisabled);
    connect(ui->checkMcp,    &QCheckBox::toggled, ui->spinMcpPort,    &QWidget::setDisabled);
    ui->spinSocketPort->setDisabled(ui->checkSocket->isChecked());
    ui->spinMcpPort->setDisabled(ui->checkMcp->isChecked());
}

DialogSettings::~DialogSettings()
{
    QSettings settings;
    settings.setValue("editSettingsGeometry", saveGeometry());

    delete ui;
}

/*
 * OK clicked: persist all five values and apply what can be applied live. The history-depth change
 * is staged in ClassWatch and takes effect on the next chip reset; server enable/port changes are
 * reconciled immediately by the controller (stop/start as needed).
 */
void DialogSettings::accept()
{
    QSettings settings;
    settings.setValue("historyDepth", ui->spinHistory->value());
    settings.setValue("socketServer", ui->checkSocket->isChecked());
    settings.setValue("socketPort", ui->spinSocketPort->value());
    settings.setValue("mcpServer", ui->checkMcp->isChecked());
    settings.setValue("mcpPort", ui->spinMcpPort->value());

    ::controller.getWatch().setHistoryDepth(ui->spinHistory->value());
    ::controller.applyServerSettings();

    QDialog::accept();
}
