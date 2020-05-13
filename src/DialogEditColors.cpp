#include "DialogEditColors.h"
#include "ui_DialogEditColors.h"
#include <QSettings>

DialogEditColors::DialogEditColors(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditColors)
{
    ui->setupUi(this);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    QSettings settings;
    restoreGeometry(settings.value("editColorsGeometry").toByteArray());
}

DialogEditColors::~DialogEditColors()
{
    QSettings settings;
    settings.setValue("editColorsGeometry", saveGeometry());

    delete ui;
}
