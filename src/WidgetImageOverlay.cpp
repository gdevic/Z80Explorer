#include "WidgetImageOverlay.h"
#include "ui_WidgetImageOverlay.h"

#include <QPushButton>
#include <QToolButton>

WidgetImageOverlay::WidgetImageOverlay(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetImageOverlay)
{
    ui->setupUi(this);

    connect(ui->editFind, SIGNAL(returnPressed()), this, SLOT(onFind()));
    connect(ui->btCoords, &QPushButton::clicked, this, &WidgetImageOverlay::actionCoords);
    connect(ui->btA, &QToolButton::clicked, this, [this](){ emit actionButton(0); } );
    connect(ui->btB, &QToolButton::clicked, this, [this](){ emit actionButton(1); } );
    connect(ui->btC, &QToolButton::clicked, this, [this](){ emit actionButton(2); } );
    connect(ui->btD, &QToolButton::clicked, this, [this](){ emit actionButton(3); } );
}

WidgetImageOverlay::~WidgetImageOverlay()
{
    delete ui;
}

void WidgetImageOverlay::setInfoLine(uint index, QString text)
{
    if ((index == 0) || (index == 1))
        ui->labelInfo1->setText(text.left(30));
    if ((index == 0) || (index == 2))
        ui->labelInfo2->setText(text.left(30));
}

void WidgetImageOverlay::clearInfoLine(uint index)
{
    if ((index == 0) || (index == 1))
        ui->labelInfo1->clear();
    if ((index == 0) || (index == 2))
        ui->labelInfo2->clear();
}

/*
 * Sets checked state to one of 4 buttons specified by the index
 */
void WidgetImageOverlay::setButton(uint i, bool checked)
{
    if (i == 0) ui->btA->setChecked(checked);
    if (i == 1) ui->btB->setChecked(checked);
    if (i == 2) ui->btC->setChecked(checked);
    if (i == 3) ui->btD->setChecked(checked);
}

/*
 * Shows the coordinate pointed to by a mouse location
 */
void WidgetImageOverlay::setCoords(int x, int y)
{
    QString coords = QString("%1,%2").arg(x).arg(y);
    ui->btCoords->setText(coords);
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
