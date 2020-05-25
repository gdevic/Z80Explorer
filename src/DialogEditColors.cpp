#include "DialogEditColors.h"
#include "ui_DialogEditColors.h"
#include "ClassColors.h"
#include "ClassController.h"
#include "WidgetEditColor.h"
#include <QSettings>

DialogEditColors::DialogEditColors(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditColors)
{
    ui->setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("editColorsGeometry").toByteArray());

    m_methods = ::controller.getColors().getMatchingMethods();

    ui->table->setMouseTracking(true);
    connect(ui->table, SIGNAL(itemSelectionChanged()), this, SLOT(onSelectionChanged()));

    for (const auto &cdef : ::controller.getColors().getColordefs())
        addItem(cdef);

    connect(ui->btAdd, &QPushButton::clicked, this, &DialogEditColors::onAdd);
    connect(ui->btUp, &QPushButton::clicked, this, &DialogEditColors::onUp);
    connect(ui->btDown, &QPushButton::clicked, this, &DialogEditColors::onDown);
    connect(ui->btEdit, &QPushButton::clicked, this, &DialogEditColors::onEdit);
    connect(ui->btRemove, &QPushButton::clicked, this, &DialogEditColors::onRemove);
    connect(ui->table, &QTableWidget::cellDoubleClicked, this, &DialogEditColors::onDoubleClicked);
}

DialogEditColors::~DialogEditColors()
{
    QSettings settings;
    settings.setValue("editColorsGeometry", saveGeometry());

    delete ui;
}

/*
 * We are processing our own accept signal, sent when user clicks on the OK button
 */
void DialogEditColors::accept()
{
    QVector<colordef> colordefs;
    for (int row = 0; row < ui->table->rowCount(); row++)
    {
        colordef cdef;
        cdef.expr = ui->table->item(row,0)->text();
        cdef.method = ui->table->item(row,1)->data(Qt::UserRole).toInt();
        cdef.color = ui->table->item(row,2)->data(Qt::UserRole).value<QColor>();
        colordefs.append(cdef);
    }
    ::controller.getColors().setColordefs(colordefs);

    QDialog::done(QDialog::Accepted);
}

/*
 * Adds a new enty in the color table
 */
void DialogEditColors::addItem(const colordef &cdef)
{
    int row = ui->table->rowCount();

    QTableWidgetItem *item0 = new QTableWidgetItem(cdef.expr);
    item0->setData(Qt::UserRole, row);

    QTableWidgetItem *item1 = new QTableWidgetItem(m_methods[cdef.method]);
    item1->setData(Qt::UserRole, cdef.method);
    item1->setFlags(Qt::ItemIsEnabled);

    QString s = QString("(%1,%2,%3)").arg(cdef.color.red()).arg(cdef.color.green()).arg(cdef.color.blue());
    QTableWidgetItem *item2 = new QTableWidgetItem(s);
    item2->setData(Qt::UserRole, QVariant::fromValue(cdef.color));
    item2->setBackground(QBrush(QColor(cdef.color)));
    item2->setFlags(Qt::ItemIsEnabled);

    ui->table->insertRow(row);
    ui->table->setItem(row, 0, item0);
    ui->table->setItem(row, 1, item1);
    ui->table->setItem(row, 2, item2);
}

/*
 * Opens up a dialog to add a new color entry
 */
void DialogEditColors::onAdd()
{
    colordef cdef {};
    WidgetEditColor edit(this, m_methods);
    edit.adjustSize(); // XXX https://stackoverflow.com/questions/49700394/qt-unable-to-set-geometry
    edit.set(cdef);
    if (edit.exec() == QDialog::Accepted)
    {
        edit.get(cdef);
        addItem(cdef);
    }
}

void DialogEditColors::swap(int index, int delta)
{
    QTableWidgetItem *i[3] { ui->table->takeItem(index+delta,0), ui->table->takeItem(index+delta,1), ui->table->takeItem(index+delta,2) };
    ui->table->removeRow(index+delta);
    ui->table->insertRow(index);
    ui->table->setItem(index,0,i[0]);
    ui->table->setItem(index,1,i[1]);
    ui->table->setItem(index,2,i[2]);
}

/*
 * Moves selected color items one slot up
 */
void DialogEditColors::onUp()
{
    Q_ASSERT(ui->table->selectedItems().count() >= 1);
    QList<QTableWidgetItem *> sel = ui->table->selectedItems();
    QVector<int> rows;
    for (auto row : sel)
        rows.append(row->row());
    std::sort(rows.begin(), rows.end(), std::less<int>());
    for (auto row : rows)
    {
        if (row == 0)
            return;
        swap(row, -1);
    }
}

/*
 * Moves selected color items one slot down
 */
void DialogEditColors::onDown()
{
    Q_ASSERT(ui->table->selectedItems().count() >= 1);
    QList<QTableWidgetItem *> sel = ui->table->selectedItems();
    QVector<int> rows;
    for (auto row : sel)
        rows.append(row->row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (auto row : rows)
    {
        if (row == (ui->table->rowCount() - 1))
            return;
        swap(row, +1);
    }
}

/*
 * Opens up a single entry edit dialog on the selected color entry
 */
void DialogEditColors::onEdit()
{
    Q_ASSERT(ui->table->selectedItems().count() == 1);
    QTableWidgetItem *selItem = ui->table->selectedItems()[0];
    int index = selItem->data(Qt::UserRole).toInt();

    colordef cdef;
    cdef.expr = selItem->text();
    cdef.method = ui->table->item(index,1)->data(Qt::UserRole).toInt();
    cdef.color = ui->table->item(index,2)->data(Qt::UserRole).value<QColor>();

    WidgetEditColor edit(this, ::controller.getColors().getMatchingMethods());
    edit.adjustSize(); // XXX https://stackoverflow.com/questions/49700394/qt-unable-to-set-geometry
    edit.set(cdef);
    if (edit.exec() == QDialog::Accepted)
    {
        edit.get(cdef);
        selItem->setText(cdef.expr);
        QString s = QString("(%1,%2,%3)").arg(cdef.color.red()).arg(cdef.color.green()).arg(cdef.color.blue());
        ui->table->item(index,1)->setText(m_methods[cdef.method]);
        ui->table->item(index,1)->setData(Qt::UserRole, cdef.method);
        ui->table->item(index,2)->setText(s);
        ui->table->item(index,2)->setBackground(QBrush(cdef.color));
        ui->table->item(index,2)->setData(Qt::UserRole, QVariant::fromValue(cdef.color));
    }
}

/*
 * Removes all selected table entries
 */
void DialogEditColors::onRemove()
{
    // Start from the end so we can use selected indices that will not change as we delete items
    for (int row = ui->table->rowCount()-1; row >= 0; row--)
    {
        if (ui->table->item(row,0)->isSelected())
            ui->table->removeRow(row);
    }
}

/*
 * Called when the user changes table items selection, correspondingly disable action buttons
 */
void DialogEditColors::onSelectionChanged()
{
    int selected = ui->table->selectedItems().count();
    ui->btUp->setEnabled(selected >= 1);
    ui->btDown->setEnabled(selected >= 1);
    ui->btEdit->setEnabled(selected == 1);
    ui->btRemove->setEnabled(selected > 0);
}

/*
 * Double-clicking on any of a color column will open the edit dialog to edit that color entry
 */
void DialogEditColors::onDoubleClicked(int row, int)
{
    // Make the double-clicked row automatically selected so it can be simply edited
    ui->table->selectRow(row);
    onEdit();
}
