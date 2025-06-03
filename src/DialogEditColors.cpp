#include "ClassColors.h"
#include "ClassController.h"
#include "DialogEditColors.h"
#include "WidgetEditColor.h"
#include "ui_DialogEditColors.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

DialogEditColors::DialogEditColors(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditColors)
{
    ui->setupUi(this);
    showFileName();

    QSettings settings;
    restoreGeometry(settings.value("editColorsGeometry").toByteArray());

    m_methods = ::controller.getColors().getMatchingMethods();

    ui->table->setMouseTracking(true);
    ui->table->resizeColumnToContents(2);
    connect(ui->table, &QTableWidget::itemSelectionChanged, this, &DialogEditColors::onSelectionChanged);

    for (const auto &cdef : ::controller.getColors().getColordefs())
        addItem(cdef);

    connect(ui->btAdd, &QPushButton::clicked, this, &DialogEditColors::onAdd);
    connect(ui->btUp, &QPushButton::clicked, this, &DialogEditColors::onUp);
    connect(ui->btDown, &QPushButton::clicked, this, &DialogEditColors::onDown);
    connect(ui->btEdit, &QPushButton::clicked, this, &DialogEditColors::onEdit);
    connect(ui->btRemove, &QPushButton::clicked, this, &DialogEditColors::onRemove);
    connect(ui->table, &QTableWidget::cellDoubleClicked, this, &DialogEditColors::onDoubleClicked);
    connect(ui->btLoad, &QPushButton::clicked, this, [=](){ onLoad(false); });
    connect(ui->btMerge, &QPushButton::clicked, this, [=](){ onLoad(true); });
    connect(ui->btSaveAs, &QPushButton::clicked, this, &DialogEditColors::onSaveAs);
}

DialogEditColors::~DialogEditColors()
{
    QSettings settings;
    settings.setValue("editColorsGeometry", saveGeometry());

    delete ui;
}

/*
 * Show the current colors file name
 */
void DialogEditColors::showFileName()
{
    setWindowTitle(QString("Edit Colors: %1").arg(::controller.getColors().getFileName()));
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
        cdef.enabled = ui->table->item(row,2)->checkState() == Qt::Checked;
        cdef.color = ui->table->item(row,3)->data(Qt::UserRole).value<QColor>();
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

    QTableWidgetItem *item2 = new QTableWidgetItem("");
    item2->setCheckState(cdef.enabled ? Qt::Checked : Qt::Unchecked);
    item2->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);

    QString s = QString("(%1,%2,%3)").arg(cdef.color.red()).arg(cdef.color.green()).arg(cdef.color.blue());
    QTableWidgetItem *item3 = new QTableWidgetItem(s);
    item3->setData(Qt::UserRole, QVariant::fromValue(cdef.color));
    item3->setBackground(QBrush(QColor(cdef.color)));
    item3->setFlags(Qt::ItemIsEnabled);

    ui->table->insertRow(row);
    ui->table->setItem(row, 0, item0);
    ui->table->setItem(row, 1, item1);
    ui->table->setItem(row, 2, item2);
    ui->table->setItem(row, 3, item3);
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
    QTableWidgetItem *i[4] { ui->table->takeItem(index+delta,0), ui->table->takeItem(index+delta,1),
                             ui->table->takeItem(index+delta,2), ui->table->takeItem(index+delta,3) };
    ui->table->removeRow(index+delta);
    ui->table->insertRow(index);
    ui->table->setItem(index,0,i[0]);
    ui->table->setItem(index,1,i[1]);
    ui->table->setItem(index,2,i[2]);
    ui->table->setItem(index,3,i[3]);
}

/*
 * Moves selected color items one slot up
 */
void DialogEditColors::onUp()
{
    Q_ASSERT(ui->table->selectedItems().count() >= 1);
    QList<QTableWidgetItem *> sel = ui->table->selectedItems();
    QVector<int> rows;
    for (auto &row : sel)
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
    for (auto &row : sel)
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
    cdef.enabled = ui->table->item(index,2)->checkState() == Qt::Checked;
    cdef.color = ui->table->item(index,3)->data(Qt::UserRole).value<QColor>();

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
        ui->table->item(index,2)->setCheckState(cdef.enabled ? Qt::Checked : Qt::Unchecked);
        ui->table->item(index,3)->setText(s);
        ui->table->item(index,3)->setBackground(QBrush(cdef.color));
        ui->table->item(index,3)->setData(Qt::UserRole, QVariant::fromValue(cdef.color));
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

/*
 * Loads custom color definition from a file
 */
void DialogEditColors::onLoad(bool merge)
{
    // Prompts the user to select which color definition file to load
    QString fileName = QFileDialog::getOpenFileName(this, "Select a color definition file to load", "", "colors (*.json);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!::controller.getColors().load(fileName, merge))
            QMessageBox::critical(this, "Error", "Selected file is not a valid color definition file");
        else
        {
            // Rebuild the color table
            ui->table->selectAll();
            onRemove();
            for (const auto &cdef : ::controller.getColors().getColordefs())
                addItem(cdef);
            showFileName();
        }
    }
}

/*
 * Saves color definition to a file
 */
void DialogEditColors::onSaveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save a color definition to a file", "", "colors (*.json);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!::controller.getColors().save(fileName))
            QMessageBox::critical(this, "Error", "Unable to save color definition to " + fileName);
        showFileName();
    }
}