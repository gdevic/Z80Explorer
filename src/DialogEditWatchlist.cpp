#include "DialogEditWatchlist.h"
#include "ui_DialogEditWatchlist.h"
#include <QSettings>

DialogEditWatchlist::DialogEditWatchlist(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditWatchlist)
{
    ui->setupUi(this);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    QSettings settings;
    restoreGeometry(settings.value("editWatchlistGeometry").toByteArray());

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
    nodeList.sort();
    ui->listAll->addItems(nodeList);
}

void DialogEditWatchlist::setWatchlist(QStringList nodeList)
{
    ui->listSelected->addItems(nodeList);
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
    for (auto item : ui->listAll->selectedItems())
    {
        if (ui->listSelected->findItems(item->text(), Qt::MatchFixedString).count()==0)
            ui->listSelected->addItem(item->text());
    }
}

void DialogEditWatchlist::onRemove()
{
    for (auto item : ui->listSelected->selectedItems())
        delete ui->listSelected->takeItem(ui->listSelected->row(item));
}
