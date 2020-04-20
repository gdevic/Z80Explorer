#include "DialogEditAnnotations.h"
#include "ui_DialogEditAnnotations.h"
#include <QSettings>

DialogEditAnnotations::DialogEditAnnotations(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditAnnotations)
{
    ui->setupUi(this);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    QSettings settings;
    restoreGeometry(settings.value("editAnnotationsGeometry").toByteArray());
}

DialogEditAnnotations::~DialogEditAnnotations()
{
    QSettings settings;
    settings.setValue("editAnnotationsGeometry", saveGeometry());

    delete ui;
}
