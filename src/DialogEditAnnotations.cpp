#include "ClassController.h"
#include "DialogEditAnnotations.h"
#include "ui_DialogEditAnnotations.h"
#include <QSettings>
#include <QStaticText>

DialogEditAnnotations::DialogEditAnnotations(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditAnnotations)
{
    ui->setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("editAnnotationsGeometry").toByteArray());

    connect(ui->listAll, &QListWidget::itemSelectionChanged, this, &DialogEditAnnotations::selChanged);
    connect(ui->btUp, &QPushButton::clicked, this, &DialogEditAnnotations::onUp);
    connect(ui->btDown, &QPushButton::clicked, this, &DialogEditAnnotations::onDown);
    connect(ui->btAdd, &QPushButton::clicked, this, &DialogEditAnnotations::onAdd);
    connect(ui->btDuplicate, &QPushButton::clicked, this, &DialogEditAnnotations::onDuplicate);
    connect(ui->btDelete, &QPushButton::clicked, this, &DialogEditAnnotations::onDelete);
    connect(ui->textEdit, &QPlainTextEdit::textChanged, this, &DialogEditAnnotations::onTextChanged);
    connect(ui->spinSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &DialogEditAnnotations::onSizeChanged);
    connect(ui->spinX, QOverload<int>::of(&QSpinBox::valueChanged), this, &DialogEditAnnotations::onXChanged);
    connect(ui->spinY, QOverload<int>::of(&QSpinBox::valueChanged), this, &DialogEditAnnotations::onYChanged);
    connect(ui->checkBar, &QCheckBox::toggled, this, &DialogEditAnnotations::onBarChanged);
    connect(ui->checkRect, &QCheckBox::toggled, this, &DialogEditAnnotations::onRectChanged);
    connect(ui->btApply, &QPushButton::clicked, this, &DialogEditAnnotations::onApply);

    m_orig = ::controller.getAnnotation().get();
    // Populate the main list widget with the given list of annotation items
    for (auto &i : ::controller.getAnnotation().get())
        append(i);
}

DialogEditAnnotations::~DialogEditAnnotations()
{
    QSettings settings;
    settings.setValue("editAnnotationsGeometry", saveGeometry());

    delete ui;
}

/*
 * Focus on the selected rows (indices)
 */
void DialogEditAnnotations::selectRows(QVector<uint> &sel)
{
    for (auto i : sel)
        ui->listAll->setCurrentRow(i, QItemSelectionModel::Toggle);
}

/*
 * User clicked on the Apply button
 */
void DialogEditAnnotations::onApply()
{
    // Rebuild the list of annotations
    QVector<annotation> list;
    for (int i=0; i < ui->listAll->count(); i++)
        list.append(get(ui->listAll->item(i)));

    ::controller.getAnnotation().set(list);
}

/*
 * We are processing our own accept signal, sent when user clicks on the OK button
 */
void DialogEditAnnotations::accept()
{
    onApply();
    QDialog::done(QDialog::Accepted);
}

/*
 * In the case of reject (user clicked the Cancel button), revert to the original list
 */
void DialogEditAnnotations::reject()
{
    ::controller.getAnnotation().set(m_orig);
    QDialog::done(QDialog::Rejected);
}

annotation DialogEditAnnotations::get(QListWidgetItem *item)
{
    QVariant data = item->data(Qt::UserRole);
    return data.value<annotation>();
}

void DialogEditAnnotations::set(QListWidgetItem *item, annotation &annot)
{
    item->setData(Qt::UserRole, QVariant::fromValue(annot));
    item->setText(annot.text);
}

void DialogEditAnnotations::append(annotation &annot)
{
    QListWidgetItem *item = new QListWidgetItem(ui->listAll);
    set(item, annot);
    ui->listAll->addItem(item);
}

/*
 * Moves selected annotations one slot up
 */
void DialogEditAnnotations::onUp()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    QVector<int> rows;
    for (auto i : sel)
        rows.append(ui->listAll->row(i));
    std::sort(rows.begin(), rows.end(), std::less<int>());
    for (auto row : rows)
    {
        if (row == 0)
            return;
        QListWidgetItem *i = ui->listAll->takeItem(row);
        ui->listAll->insertItem(row - 1, i);
        ui->listAll->setCurrentRow(row - 1);
    }
}

/*
 * Moves selected annotations one slot down
 */
void DialogEditAnnotations::onDown()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    QVector<int> rows;
    for (auto i : sel)
        rows.append(ui->listAll->row(i));
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (auto row : rows)
    {
        if (row == (ui->listAll->count() - 1))
            return;
        QListWidgetItem *i = ui->listAll->takeItem(row);
        ui->listAll->insertItem(row + 1, i);
        ui->listAll->setCurrentRow(row + 1);
    }
}

void DialogEditAnnotations::onAdd()
{
    annotation a("NEW");
    append(a);
}

void DialogEditAnnotations::onDuplicate()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    Q_ASSERT(sel.count() == 1);
    annotation a = get(sel[0]);
    a.pos += QPoint(a.pix / 2, a.pix / 2);
    append(a);
}

void DialogEditAnnotations::onDelete()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    Q_ASSERT(sel.size() > 0);
    for (auto i : sel)
        delete ui->listAll->takeItem(ui->listAll->row(i));
}

/*
 * Use selection changed signal to enable or disable buttons based on their function
 */
void DialogEditAnnotations::selChanged()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    ui->textEdit->setEnabled(sel.count() == 1);
    ui->spinSize->setEnabled(sel.count() > 0);
    ui->spinX->setEnabled(sel.count() > 0);
    ui->spinY->setEnabled(sel.count() > 0);
    ui->checkBar->setEnabled(sel.count() > 0);
    ui->checkRect->setEnabled(sel.count() > 0);
    ui->btUp->setEnabled(sel.count() > 0);
    ui->btDown->setEnabled(sel.count() > 0);
    ui->btDuplicate->setEnabled(sel.count() == 1);
    ui->btDelete->setEnabled(sel.count() > 0);
    if (sel.count() > 0)
    {
        annotation annot = get(sel[0]);
        if (sel.count() == 1) // Set these fields only if a single annotation has been selected
        {
            ui->textEdit->setPlainText(annot.text); // Show the text
            ui->spinX->setSingleStep(annot.pix / 4); // Set the X,Y location steps to a fraction of pix
            ui->spinY->setSingleStep(annot.pix / 4);
        }
        ui->spinSize->setValue(annot.pix);
        ui->spinX->setValue(annot.pos.x());
        ui->spinY->setValue(annot.pos.y());
        ui->checkBar->setCheckState(annot.overline ? Qt::Checked : Qt::Unchecked);
        ui->checkRect->setCheckState(annot.drawrect ? Qt::Checked : Qt::Unchecked);
    }
}

/*
 * User changed the text for a single annotation
 * Update the list view and also the node that this listview holds
 */
void DialogEditAnnotations::onTextChanged()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    Q_ASSERT(sel.count() == 1);
    sel[0]->setText(ui->textEdit->toPlainText());
    annotation a = get(sel[0]);
    a.text = ui->textEdit->toPlainText();
    set(sel[0], a);
}

void DialogEditAnnotations::onSizeChanged()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    int value = ui->spinSize->value();
    for (auto i : sel)
    {
        annotation a = get(i);
        a.pix = value;
        set(i, a);
    }
}

void DialogEditAnnotations::onXChanged()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    int value = ui->spinX->value();
    for (auto i : sel)
    {
        annotation a = get(i);
        a.pos.setX(value);
        set(i, a);
    }
}

void DialogEditAnnotations::onYChanged()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    int value = ui->spinY->value();
    for (auto i : sel)
    {
        annotation a = get(i);
        a.pos.setY(value);
        set(i, a);
    }
}

void DialogEditAnnotations::onBarChanged()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    bool value = ui->checkBar->checkState() == Qt::Checked;
    for (auto i : sel)
    {
        annotation a = get(i);
        a.overline = value;
        set(i, a);
    }
}

void DialogEditAnnotations::onRectChanged()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    bool value = ui->checkRect->checkState() == Qt::Checked;
    for (auto i : sel)
    {
        annotation a = get(i);
        a.drawrect = value;
        set(i, a);
    }
}
