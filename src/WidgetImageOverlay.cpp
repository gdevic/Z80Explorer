#include "WidgetImageOverlay.h"
#include "ui_WidgetImageOverlay.h"

WidgetImageOverlay::WidgetImageOverlay(QWidget *parent, QString sid) :
    QWidget(parent),
    ui(new Ui::WidgetImageOverlay)
{
    ui->setupUi(this);
    setWhatsThis(sid);

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

/*
 * Called at init time to create a list of push buttons, each corresponding to one image from the list
 */
void WidgetImageOverlay::createImageButtons(QStringList imageNames)
{
    static const QString c = "123456789abcdefghijklmnopq";
    for (uint i=0; i < imageNames.count(); i++)
    {
        QPushButton *p = new QPushButton(this);
        m_imageButtons.append(p);
        p->setStyleSheet("text-align:left;");
        p->setText(QString(c[i % c.length()]) + " ... " + imageNames[i]);
        connect(p, &QPushButton::clicked, this, [this, i]()
                {
                    bool ctrl = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
                    emit actionSetImage(i, ctrl);
                });
        ui->layout->addWidget(p);
    }
    ui->layout->setSizeConstraint(QLayout::SetMinimumSize);
}

/*
 * Returns a string containing the status of the image layer visibility
 */
QString WidgetImageOverlay::getLayers()
{
    QString layers;
    for (uint i=0; i < m_imageButtons.size(); i++)
        layers.append((m_imageButtons[i]->isFlat()) ? '1' : '0');
    return layers;
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
void WidgetImageOverlay::selectImageButton(uint img, bool blend)
{
    for (uint i=0; i < m_imageButtons.count(); i++)
    {
        QPushButton *pb = m_imageButtons[i];
        if (i == img)
            pb->setFlat(!pb->isFlat() || !blend);
        else if (!blend) // If we are not blending, reset other buttons
            pb->setFlat(false);
    }
}
