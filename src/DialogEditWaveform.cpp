#include "DialogEditWaveform.h"
#include "ui_DialogEditWaveform.h"
#include "ClassController.h"

DialogEditWaveform::DialogEditWaveform(QWidget *parent, QVector<viewitem> list) :
    QDialog(parent),
    ui(new Ui::DialogEditWaveform),
    m_view(list)
{
    ui->setupUi(this);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    ui->listAll->addItems(::controller.getWatch().getWatchlist());

    connect(ui->listAll, SIGNAL(itemSelectionChanged()), this, SLOT(allSelChanged()));
    connect(ui->listView, SIGNAL(itemSelectionChanged()), this, SLOT(viewSelChanged()));
    connect(ui->btAdd, &QPushButton::clicked, this, &DialogEditWaveform::onAdd);
    connect(ui->btRemove, &QPushButton::clicked, this, &DialogEditWaveform::onRemove);
    connect(ui->btUp, &QPushButton::clicked, this, &DialogEditWaveform::onUp);
    connect(ui->btDown, &QPushButton::clicked, this, &DialogEditWaveform::onDown);
}

DialogEditWaveform::~DialogEditWaveform()
{
    delete ui;
}

void DialogEditWaveform::showEvent(QShowEvent *)
{
    for (auto i : m_view)
        ui->listView->addItem(i.name);
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
 * Use selection changed signal to enable or disable buttons pertaining our view list
 */
void DialogEditWaveform::viewSelChanged()
{
    QList<QListWidgetItem *> sel = ui->listView->selectedItems();
    ui->btRemove->setEnabled(sel.size());
    ui->btUp->setEnabled(sel.size());
    ui->btDown->setEnabled(sel.size());
    ui->comboFormat->setEnabled(sel.size());
    ui->btColor->setEnabled(sel.size());
}

// XXX There must be a way to simply use m_view.contains(name) !
bool DialogEditWaveform::contains(QString name)
{
    for (auto i : m_view)
        if (i.name == name)
            return true;
    return false;
}

/*
 * Adds selected signals from the available to our view list
 */
void DialogEditWaveform::onAdd()
{
    QList<QListWidgetItem *> sel = ui->listAll->selectedItems();
    for (auto i : sel)
    {
        const QString name = i->text();
        if (!contains(name))
        {
            ui->listView->addItem(name);
            m_view.append(name);
        }
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
    {
        m_view.removeOne(viewitem(i->text()));
        delete ui->listView->takeItem(ui->listView->row(i));
    }
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
        m_view.swapItemsAt(row, row - 1);
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
        m_view.swapItemsAt(row, row+1);
        QListWidgetItem *i = ui->listView->takeItem(row);
        ui->listView->insertItem(row + 1, i);
        ui->listView->setCurrentRow(row + 1);
    }
}
