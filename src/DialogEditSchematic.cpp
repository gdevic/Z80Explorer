#include "DialogEditSchematic.h"
#include "ui_DialogEditSchematic.h"
#include <QFile>
#include <QSettings>

// Initial and failsafe list of terminating nodes
static QString termNodes("pla*, _pla*, t*, m*");

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

    // Clicking on the Reload button reloads the list of terminating nodes from the ini file
    connect(ui->btReload, &QPushButton::clicked, this, [=]()
    {
        if (load("schem.ini", termNodes))
            ui->editTermNodes->setPlainText(termNodes);
    });
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

    // Load the list of terminating nodes but apply it only if the settings does not have it
    load("schem.ini", termNodes);

    if (!settings.contains("schematicTermNodes"))
        settings.setValue("schematicTermNodes", termNodes);
}

/*
 * Load the list of terminating nodes from the ini file
 */
bool DialogEditSchematic::load(const QString &fileName, QString &loadedText)
{
    qInfo() << "Loading schematic' terminating nets from" << fileName;
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QStringList validLines;
        QTextStream in(&file);
        while (!in.atEnd())
        {
            QString line = in.readLine();
            QString trimmedLine = line.trimmed();
            // Ignore comment lines or empty lines
            if (trimmedLine.startsWith(';') || trimmedLine.isEmpty())
                continue;
            validLines.append(line);
        }
        file.close();
        loadedText = validLines.join("\n");
        return true;
    }
    else
        qWarning() << "Unable to load" << fileName;
    return false;
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
