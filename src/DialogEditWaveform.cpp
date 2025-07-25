#include "ClassController.h"
#include "DialogEditWaveform.h"
#include "ui_DialogEditWaveform.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QSettings>

DialogEditWaveform::DialogEditWaveform(QWidget *parent, QVector<viewitem> list) :
    QDialog(parent),
    ui(new Ui::DialogEditWaveform)
{
    ui->setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("editWaveformGeometry").toByteArray());

    QStringList watches = ::controller.getWatch().getWatchlist();
    watches.sort();
    ui->listAll->addItems(watches);

    connect(ui->listAll, &QListWidget::itemSelectionChanged, this, &DialogEditWaveform::allSelChanged);
    connect(ui->listView, &QListWidget::itemSelectionChanged, this, &DialogEditWaveform::viewSelChanged);
    connect(ui->btAdd, &QPushButton::clicked, this, &DialogEditWaveform::onAdd);
    connect(ui->btRemove, &QPushButton::clicked, this, &DialogEditWaveform::onRemove);
    connect(ui->btUp, &QPushButton::clicked, this, &DialogEditWaveform::onUp);
    connect(ui->btDown, &QPushButton::clicked, this, &DialogEditWaveform::onDown);
    connect(ui->btColor, &QPushButton::clicked, this, &DialogEditWaveform::onColor);
    connect(ui->comboFormat, QOverload<int>::of(&QComboBox::activated), this, &DialogEditWaveform::onFormatIndexChanged);
    connect(ui->btSaveAs, &QPushButton::clicked, this, &DialogEditWaveform::onSaveAs);

    // Populate the view list widget with the given list of view items
    for (auto &i : list)
        append(i);
}

DialogEditWaveform::~DialogEditWaveform()
{
    QSettings settings;
    settings.setValue("editWaveformGeometry", saveGeometry());

    delete ui;
}

/*
 * Returns (by setting "list") the edited list of view items
 */
void DialogEditWaveform::getList(QVector<viewitem> &list)
{
    list.clear();
    for (int i = 0; i < ui->listView->count(); i++)
    {
        QListWidgetItem *item = ui->listView->item(i);
        viewitem view = get(item);
        list.append(view);
    }
}

viewitem DialogEditWaveform::get(QListWidgetItem *item)
{
    QVariant data = item->data(Qt::UserRole);
    return data.value<viewitem>();
}

void DialogEditWaveform::set(QListWidgetItem *item, viewitem &view)
{
    item->setData(Qt::UserRole, QVariant::fromValue(view));
    item->setText(view.name);
}

void DialogEditWaveform::append(viewitem &view)
{
    QListWidgetItem *item = new QListWidgetItem(ui->listView);
    set(item, view);
    ui->listView->addItem(item);
}

/*
 * Use selection changed signal to enable or disable Add button
 */
void DialogEditWaveform::allSelChanged()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    ui->btAdd->setEnabled(sel.size());
}

/*
 * Use selection changed signal to enable or disable buttons based on their function
 */
void DialogEditWaveform::viewSelChanged()
{
    QList<QListWidgetItem *> sel = ui->listView->selectedItems();
    ui->btRemove->setEnabled(sel.size());
    ui->btUp->setEnabled(sel.size());
    ui->btDown->setEnabled(sel.size());
    ui->btColor->setEnabled(sel.size());
    ui->comboFormat->setEnabled(sel.size());
    ui->comboFormat->clear();
    if (sel.size())
    {
        viewitem view = get(sel[0]);
        // Store the format value since addItems will call IndexChanged and reset it in the widget
        uint format = view.format;
        ui->comboFormat->addItems(::controller.getFormats(view.name));
        ui->comboFormat->setCurrentIndex(format);
    }
    ui->btSaveAs->setEnabled(sel.size());
}

void DialogEditWaveform::onFormatIndexChanged(int index)
{
    if (index >= 0)
    {
        QList<QListWidgetItem *> sel = ui->listView->selectedItems();
        for (auto &i : sel)
        {
            viewitem view = get(i);
            view.format = index;
            set(i, view);
        }
    }
}

/*
 * Adds selected signals from the available to our view list
 * Allow multiple instances of a signal to be added
 */
void DialogEditWaveform::onAdd()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    for (auto i : sel)
    {
        viewitem view(i->text());
        view.net = ::controller.getNetlist().get(i->text());
        append(view);
    }
}

/*
 * Removes selected signals from the view
 */
void DialogEditWaveform::onRemove()
{
    QList<QListWidgetItem *> sel = ui->listView->selectedItems();
    Q_ASSERT(sel.size() > 0);
    for (auto i : sel)
        delete ui->listView->takeItem(ui->listView->row(i));
}

/*
 * Moves selected signal one slot up
 */
void DialogEditWaveform::onUp()
{
    QList<QListWidgetItem *> sel = ui->listView->selectedItems();
    QVector<int> rows;
    for (auto i : sel)
        rows.append(ui->listView->row(i));
    std::sort(rows.begin(), rows.end(), std::less<int>());
    for (auto row : rows)
    {
        if (row == 0)
            return;
        QListWidgetItem *i = ui->listView->takeItem(row);
        ui->listView->insertItem(row - 1, i);
        ui->listView->setCurrentRow(row - 1);
    }
}

/*
 * Moves selected signal one slot down
 */
void DialogEditWaveform::onDown()
{
    QList<QListWidgetItem *> sel = ui->listView->selectedItems();
    QVector<int> rows;
    for (auto i : sel)
        rows.append(ui->listView->row(i));
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (auto row : rows)
    {
        if (row == (ui->listView->count() - 1))
            return;
        QListWidgetItem *i = ui->listView->takeItem(row);
        ui->listView->insertItem(row + 1, i);
        ui->listView->setCurrentRow(row + 1);
    }
}

/*
 * Change signal's waveform color
 */
void DialogEditWaveform::onColor()
{
    QList<QListWidgetItem *> sel = ui->listView->selectedItems();
    QStringList selected;
    for (auto i : sel)
        selected.append(i->text());
    viewitem view = get(sel[0]);
    QColor col = view.color;
    col = QColorDialog::getColor(col); // Opens a dialog to let the user chose a color
    if (col.isValid())
    {
        for (auto &i : sel)
        {
            viewitem view = get(i);
            view.color = col;
            set(i, view);
        }
    }
}

/*
 * Saves selected waveform items to a file
 * Note: Saving of waveform items function is duplicated in DockWaveform.cpp where we save all items
 */
void DialogEditWaveform::onSaveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save selected waveform items to a file", "", "waveform (*.json);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        qInfo() << "Saving selected waveform items to" << fileName;
        QList<QListWidgetItem *> sel = ui->listView->selectedItems();

        QFile saveFile(fileName);
        if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
        {
            QJsonObject json;
            QJsonArray jsonArray;
            for (auto vi : sel)
            {
                viewitem a = get(vi);
                QJsonObject obj;
                obj["name"] = a.name;
                obj["net"] = a.net;
                obj["format"] = int(a.format);
                obj["color"] = QString("%1,%2,%3,%4").arg(a.color.red()).arg(a.color.green()).arg(a.color.blue()).arg(a.color.alpha());
                jsonArray.append(obj);
            }
            json["waveform"] = jsonArray;

            QJsonDocument saveDoc(json);
            saveFile.write(saveDoc.toJson());
        }
        else
            QMessageBox::critical(this, "Error", "Unable to save waveform items to " + fileName);
    }
}
