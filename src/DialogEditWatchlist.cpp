#include "ClassController.h"
#include "DialogEditWatchlist.h"
#include "ui_DialogEditWatchlist.h"
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
    connect(ui->btAddAll, &QPushButton::clicked, this, &DialogEditWatchlist::onAddAll);
    connect(ui->btRemove, &QPushButton::clicked, this, &DialogEditWatchlist::onRemove);
    connect(ui->btRemoveAll, &QPushButton::clicked, this, &DialogEditWatchlist::onRemoveAll);
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
        ui->listAll->addItem(getListItem(Net, name));
    ui->btAddAll->setEnabled(!nodeList.isEmpty());
}

void DialogEditWatchlist::setWatchlist(QStringList nodeList)
{
    // Read all nets and buses and separate nets from buses
    ClassNetlist &Net = ::controller.getNetlist();
    for (auto &name : nodeList)
        ui->listSelected->addItem(getListItem(Net, name));
    ui->btRemoveAll->setEnabled(!nodeList.isEmpty());
}

QListWidgetItem *DialogEditWatchlist::getListItem(ClassNetlist &Net, QString name)
{
    QListWidgetItem *li = new QListWidgetItem(name);
    net_t net = Net.get(name); // Get net number, zero net number is a bus
    if (net)
        li->setToolTip(QString("net %1").arg(net));
    else
    {
        const QVector<net_t> &bus = Net.getBus(name);
        QStringList nets;
        for (auto n : bus)
            nets.append(Net.get(n));
        li->setToolTip(nets.join(','));
    }
    return li;
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
        // Make sure we don't add duplicate entries
        if (ui->listSelected->findItems(item->text(), Qt::MatchFixedString).count()==0)
        {
            QListWidgetItem *li = new QListWidgetItem(item->text());
            li->setToolTip(item->toolTip());
            ui->listSelected->addItem(li);
        }
    }
    ui->btRemoveAll->setEnabled(ui->listSelected->count());
}

void DialogEditWatchlist::onAddAll()
{
    ui->listSelected->clear();
    ui->listAll->selectAll();
    onAdd();
    ui->listAll->clearSelection();
}

void DialogEditWatchlist::onRemove()
{
    for (auto &item : ui->listSelected->selectedItems())
        delete ui->listSelected->takeItem(ui->listSelected->row(item));
    ui->btRemoveAll->setEnabled(ui->listSelected->count());
}

void DialogEditWatchlist::onRemoveAll()
{
    ui->listSelected->clear();
    ui->btRemoveAll->setEnabled(false);
}

void DialogEditWatchlist::allSelChanged()
{
    QVector<QListWidgetItem *> sel = ui->listAll->selectedItems().toVector();
    ui->btAdd->setEnabled(sel.size() > 0);
    if (sel.size() == 1)
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
    if (sel.size() == 1)
    {
        ui->labelNets->setText(sel[0]->toolTip());
        ui->labelNets->setAlignment(Qt::AlignRight);
    }
    else
        ui->labelNets->clear();
}
