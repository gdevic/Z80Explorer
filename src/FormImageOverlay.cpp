#include "FormImageOverlay.h"
#include "ui_FormImageOverlay.h"

FormImageOverlay::FormImageOverlay(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormImageOverlay)
{
    ui->setupUi(this);
}

FormImageOverlay::~FormImageOverlay()
{
    delete ui;
}

/*
 * Shows the coordinate and color pointed to by a mouse location
 */
void FormImageOverlay::onPointerData(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    QString s = QString("x=%1 y=%2 (%3,%4,%5)").arg(x).arg(5000-1-y).arg(r).arg(g).arg(b);
    ui->label1->setText(s);
}

void FormImageOverlay::onClearPointerData()
{
    ui->label1->setText("-");
}

void FormImageOverlay::on_btBuild_clicked()
{
    emit actionBuild();
}

void FormImageOverlay::setText(int index, QString text)
{
    if (index == 1) ui->label2->setText(text);
    if (index == 2) ui->label3->setText(text);
}
