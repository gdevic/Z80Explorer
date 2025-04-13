#include "DialogEditSchematic.h"
#include "ui_DialogEditSchematic.h"
#include <QSettings>

// Default list of terminating nodes
// XXX This list should probably be part of the Z80 json file set and not hard coded into the app's settings
static const QString termNodes =
    "ubus*, vbus*, pla*, _pla*, ab*, db*,\n"
    "t1, t2, t3, t4, t5, t6, m1, m2, m3, m4, m5, m6,\n"
    "int_reset";

DialogEditSchematic::DialogEditSchematic(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogEditSchematic)
{
    ui->setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("editSchematicGeometry").toByteArray());

    ui->spinDepth->setValue(settings.value("schematicMaxDepth").toInt());
    ui->checkOptClockGate->setChecked(settings.value("schematicOptClockGate").toBool());
    ui->checkOptCoalesce->setChecked(settings.value("schematicOptCoalesce").toBool());
    ui->checkOptIntermediate->setChecked(settings.value("schematicOptIntermediate").toBool());
    ui->checkOptInverters->setChecked(settings.value("schematicOptInverters").toBool());
    ui->checkOptSingleInput->setChecked(settings.value("schematicOptSingleInput").toBool());
    ui->editTermNodes->setPlainText(settings.value("schematicTermNodes").toString());

    // Clicking on the Reset button reverts the list of terminating nodes to the default suggested list
    connect(ui->btReset, &QPushButton::clicked, this, [=]()
            { ui->editTermNodes->setPlainText(termNodes);} );
}

DialogEditSchematic::~DialogEditSchematic()
{
    QSettings settings;
    settings.setValue("editSchematicGeometry", saveGeometry());

    delete ui;
}

/*
 * Initialize default schematics properties in the settings if they don't exist
 */
void DialogEditSchematic::init()
{
    QSettings settings;

    if (!settings.contains("schematicMaxDepth"))
        settings.setValue("schematicMaxDepth", 30);

    if (!settings.contains("schematicOptClockGate"))
        settings.setValue("schematicOptClockGate", true);

    if (!settings.contains("schematicOptCoalesce"))
        settings.setValue("schematicOptCoalesce", true);

    if (!settings.contains("schematicOptIntermediate"))
        settings.setValue("schematicOptIntermediate", true);

    if (!settings.contains("schematicOptInverters"))
        settings.setValue("schematicOptInverters", true);

    if (!settings.contains("schematicOptSingleInput"))
        settings.setValue("schematicOptSingleInput", true);

    if (!settings.contains("schematicTermNodes"))
        settings.setValue("schematicTermNodes", termNodes);
}

/*
 * We are processing our own accept signal, sent when user clicks on the OK button
 */
void DialogEditSchematic::accept()
{
    QSettings settings;

    settings.setValue("schematicMaxDepth", ui->spinDepth->value());
    settings.setValue("schematicOptClockGate", ui->checkOptClockGate->isChecked());
    settings.setValue("schematicOptCoalesce", ui->checkOptCoalesce->isChecked());
    settings.setValue("schematicOptIntermediate", ui->checkOptIntermediate->isChecked());
    settings.setValue("schematicOptInverters", ui->checkOptInverters->isChecked());
    settings.setValue("schematicOptSingleInput", ui->checkOptSingleInput->isChecked());
    settings.setValue("schematicTermNodes", ui->editTermNodes->toPlainText());

    QDialog::done(QDialog::Accepted);
}
