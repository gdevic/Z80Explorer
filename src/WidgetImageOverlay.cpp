#include "WidgetImageOverlay.h"
#include "ui_WidgetImageOverlay.h"

#include <QLabel>

WidgetImageOverlay::WidgetImageOverlay(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetImageOverlay)
{
    ui->setupUi(this);

    connect(ui->editFind, SIGNAL(returnPressed()), this, SLOT(onFind()));
}

WidgetImageOverlay::~WidgetImageOverlay()
{
    delete ui;
}

void WidgetImageOverlay::setLayerNames(QStringList layers)
{
    const QString c = "123456789abcdefghijklmnopq";
    QVBoxLayout *layout = new QVBoxLayout(this);
    for (int i=0; i < layers.count(); i++)
    {
        QLabel *p = new QLabel(this);
        p->setText(QString(c[i % c.length()]) + " ... " + layers[i]);
        layout->addWidget(p);
    }
    ui->verticalLayout_2->addLayout(layout);
    ui->verticalLayout_2->setSizeConstraint(QLayout::SetMinimumSize);
}

/*
 * Shows the coordinate pointed to by a mouse location
 */
void WidgetImageOverlay::onPointerData(int x, int y)
{
    QString coords = QString("%1,%2").arg(x).arg(y);
    ui->btCoords->setText(coords);
}

void WidgetImageOverlay::onClearPointerData()
{
    ui->label1->setText(QString());
}

void WidgetImageOverlay::on_btTraces_clicked()
{
    emit actionTraces();
}

void WidgetImageOverlay::on_btCoords_clicked()
{
    emit actionCoords();
}

void WidgetImageOverlay::setText(int index, QString text)
{
    if (index == 1) ui->label1->setText(text);
    if (index == 2) ui->label2->setText(text);
    if (index == 3) ui->label3->setText(text);
    if (index == 4) ui->label4->setText(text);
}

/*
 * Called by the editFind edit widget when the user presses the Enter key
 */
void WidgetImageOverlay::onFind()
{
    QString text = ui->editFind->text().trimmed();
    ui->editFind->clear(); // Clear the edit box from the user input
    emit actionFind(text);
}
