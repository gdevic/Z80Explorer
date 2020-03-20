#include "FormImageOverlay.h"
#include "ui_FormImageOverlay.h"

#include <QLabel>

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

void FormImageOverlay::setLayerNames(QStringList layers)
{
    const QString c = "123456789abcdefghijklmnopq";
    for (int i=0; i < layers.count(); i++)
    {
        QLabel *p = new QLabel(this);
        p->setText(QString(c[i % c.length()]) + " ... " + layers[i]);
        ui->verticalLayout->addWidget(p);
    }
}

/*
 * Shows the coordinate and color pointed to by a mouse location
 */
void FormImageOverlay::onPointerData(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    QString s = QString("x=%1 y=%2 (%3,%4,%5)").arg(x).arg(5000-1-y).arg(r).arg(g).arg(b);
    setText(1,s);
}

void FormImageOverlay::onClearPointerData()
{
    ui->label1->setText(QString());
}

void FormImageOverlay::on_btBuild_clicked()
{
    emit actionBuild();
}

void FormImageOverlay::setText(int index, QString text)
{
    if (index == 1) ui->label1->setText(text);
    if (index == 2) ui->label2->setText(text);
    if (index == 3) ui->label3->setText(text);
    if (index == 4) ui->label4->setText(text);
}
