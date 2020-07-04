#include "DialogEditWatchlist.h"
#include "ui_DialogEditWatchlist.h"
#include "ClassController.h"
#include <QSettings>

DialogEditWatchlist::DialogEditWatchlist(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditWatchlist)
{
    ui->setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("editWatchlistGeometry").toByteArray());

    connect(ui->listAll, &QListWidget::itemSelectionChanged, this, &DialogEditWatchlist::allSelChanged);
    connect(ui->listSelected, &QListWidget::itemSelectionChanged, this, &DialogEditWatchlist::listSelChanged);
    connect(ui->btAdd, &QPushButton::clicked, this, &DialogEditWatchlist::onAdd);
    connect(ui->btRemove, &QPushButton::clicked, this, &DialogEditWatchlist::onRemove);
}

DialogEditWatchlist::~DialogEditWatchlist()
{
    QSettings settings;
    settings.setValue("editWatchlistGeometry", saveGeometry());

    delete ui;
}

void DialogEditWatchlist::setNodeList(QStringList nodeList)
{
    nodeList.sort(); // Sort the incoming list of nets and buses to place buses at the top (they are uppercased)
    ClassNetlist &Net = ::controller.getNetlist();
    // Loop over the list and for each bus add its constituent nets to the tooltip field so we can show it
    for (auto &name : nodeList)
    {
        QListWidgetItem *li = new QListWidgetItem(name);
        if (!Net.get(name)) // Zero net number is a bus
        {
            const QVector<net_t> &bus = Net.getBus(name);
            QStringList nets;
            for (auto n : bus)
                nets.append(Net.get(n));
            li->setToolTip(nets.join(','));
        }
        ui->listAll->addItem(li);
    }
}

void DialogEditWatchlist::setWatchlist(QStringList nodeList)
{
    // Read all nets and buses and separate nets from buses
    ClassNetlist &Net = ::controller.getNetlist();
    for (auto &name : nodeList)
    {
        if (Net.get(name)) // Non-zero net number is a net
            ui->listSelected->addItem(name);
        else // Zero net number is a bus
        {
            const QVector<net_t> &bus = Net.getBus(name);
            QStringList nets;
            for (auto n : bus)
                nets.append(Net.get(n));
            add(name, nets);
        }
    }
}

void DialogEditWatchlist::add(QString busName, QStringList nets)
{
    QListWidgetItem *li = new QListWidgetItem(busName);
    li->setToolTip(nets.join(','));
    ui->listSelected->addItem(li);
}

QStringList DialogEditWatchlist::getWatchlist()
{
    QStringList list {};
    for (int i=0; i<ui->listSelected->count(); i++)
        list.append(ui->listSelected->item(i)->text());
    return list;
}

void DialogEditWatchlist::onAdd()
{
    for (auto &item : ui->listAll->selectedItems())
    {
        if (ui->listSelected->findItems(item->text(), Qt::MatchFixedString).count()==0)
            ui->listSelected->addItem(item->text());
    }
}

void DialogEditWatchlist::onRemove()
{
    for (auto &item : ui->listSelected->selectedItems())
        delete ui->listSelected->takeItem(ui->listSelected->row(item));
}

void DialogEditWatchlist::allSelChanged()
{
    QVector<QListWidgetItem *> sel = ui->listAll->selectedItems().toVector();
    ui->btAdd->setEnabled(sel.size() > 0);
    if (sel.size())
    {
        ui->labelNets->setText(sel[0]->toolTip());
        ui->labelNets->setAlignment(Qt::AlignLeft);
    }
    else
        ui->labelNets->clear();
}

void DialogEditWatchlist::listSelChanged()
{
    QVector<QListWidgetItem *> sel = ui->listSelected->selectedItems().toVector();
    ui->btRemove->setEnabled(sel.size() > 0);
    if (sel.size())
    {
        ui->labelNets->setText(sel[0]->toolTip());
        ui->labelNets->setAlignment(Qt::AlignRight);
    }
    else
        ui->labelNets->clear();
}
