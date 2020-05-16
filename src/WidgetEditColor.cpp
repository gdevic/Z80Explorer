#include "WidgetEditColor.h"
#include "ui_WidgetEditColor.h"
#include <QColorDialog>

WidgetEditColor::WidgetEditColor(QWidget *parent, QStringList methods) :
    QDialog(parent),
    ui(new Ui::WidgetEditColor)
{
    ui->setupUi(this);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    ui->comboBox->addItems(methods);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    ui->lineEdit->setFocus();

    connect(ui->btColor, &QPushButton::clicked, this, &WidgetEditColor::onColor);
    connect(ui->lineEdit, &QLineEdit::textChanged, this, &WidgetEditColor::onTextChanged);
}

WidgetEditColor::~WidgetEditColor()
{
    delete ui;
}

/*
 * Sets a single custom color matching definition to be edited
 */
void WidgetEditColor::set(const colordef &cdef)
{
    m_color = cdef.color;
    ui->lineEdit->setText(cdef.expr);
    ui->comboBox->setCurrentIndex(cdef.method);
    ui->btColor->setStyleSheet(style(m_color));
}

/*
 * Returns edited custom color matching definition
 */
void WidgetEditColor::get(colordef &cdef)
{
    cdef.expr = ui->lineEdit->text().trimmed();
    cdef.method = ui->comboBox->currentIndex();
    cdef.color = m_color;
}

/*
 * Handler for the color selection button
 */
void WidgetEditColor::onColor()
{
    // We don't use static version of QColorDialog::getColor() because it generates a QWindowsWindow::setGeometry warning on Windows
    QColorDialog *dlgColor = new QColorDialog(m_color);
    dlgColor->adjustSize(); // XXX https://stackoverflow.com/questions/49700394/qt-unable-to-set-geometry
    dlgColor->exec();
    QColor color = dlgColor->selectedColor();
    if (color.isValid())
    {
        m_color = color;
        ui->btColor->setStyleSheet(style(m_color));
    }
    delete dlgColor;
}

void WidgetEditColor::onTextChanged(const QString &text)
{
    // If the expr field is empty, disable the OK button so we don't accept the entry
    bool valid = text.trimmed().length() > 0;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
    // If the text starts with a number, automatically assign the direct net matching method
    // Note: This hard-codes the matching method index!
    if (valid && text.trimmed()[0].isNumber())
        ui->comboBox->setCurrentIndex(3); // XXX Hard-coded matching index!
}
