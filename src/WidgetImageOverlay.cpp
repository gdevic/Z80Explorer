#include "WidgetImageOverlay.h"
#include "ui_WidgetImageOverlay.h"

#include <QPushButton>

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

void WidgetImageOverlay::setImageNames(QStringList images)
{
    const QString c = "123456789abcdefghijklmnopq";
    for (int i=0; i < images.count(); i++)
    {
        QPushButton *p = new QPushButton(this);
        p->setStyleSheet("text-align:left;");
        p->setText(QString(c[i % c.length()]) + " ... " + images[i]);
        connect(p, &QPushButton::clicked, this, [this, i]() { emit actionSetImage(i); });
        ui->layout->addWidget(p);
    }
    ui->layout->setSizeConstraint(QLayout::SetMinimumSize);
}

/*
 * Shows the coordinate pointed to by a mouse location
 */
void WidgetImageOverlay::onPointerData(int x, int y)
{
    QString coords = QString("%1,%2").arg(x).arg(y);
    ui->btCoords->setText(coords);
}

void WidgetImageOverlay::on_btCoords_clicked()
{
    emit actionCoords();
}

void WidgetImageOverlay::setInfoLine(QString text)
{
    ui->labelInfo->setText(text);
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

/*
 * Called when an image is selected to highlight the corresponding button
 * If compose is true, other buttons will not be reset (additive operation)
 */
void WidgetImageOverlay::selectImage(QString name, bool compose)
{
    for (auto &pb : findChildren<QPushButton *>())
        pb->setFlat((name == pb->text().mid(6)) || (pb->isFlat() & compose));
}
