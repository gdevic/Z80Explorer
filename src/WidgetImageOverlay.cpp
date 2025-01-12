#include "WidgetImageOverlay.h"
#include "ui_WidgetImageOverlay.h"

WidgetImageOverlay::WidgetImageOverlay(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetImageOverlay)
{
    ui->setupUi(this);

    connect(ui->editFind, &QLineEdit::returnPressed, this, &WidgetImageOverlay::onFind);
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
    if ((index == 0) || (index == 3))
        ui->labelInfo3->setText(text.left(30));
}

void WidgetImageOverlay::clearInfoLine(uint index)
{
    if ((index == 0) || (index == 1))
        ui->labelInfo1->clear();
    if ((index == 0) || (index == 2))
        ui->labelInfo2->clear();
    if ((index == 0) || (index == 3))
        ui->labelInfo3->clear();
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
 * Shows the text shown on the coordinate button
 */
void WidgetImageOverlay::setCoords(const QString coords)
{
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
 * If blend is true, other buttons will not be reset
 */
void WidgetImageOverlay::selectImage(QString name, bool blend)
{
    for (auto &pb : findChildren<QPushButton *>())
    {
        if (name == pb->text().mid(6))
            pb->setFlat(!pb->isFlat() || !blend);
        else if (!blend) // If we are not blending, reset other buttons
            pb->setFlat(false);
    }
}
