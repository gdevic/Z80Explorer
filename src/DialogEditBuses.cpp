#include "DialogEditBuses.h"
#include "ui_DialogEditBuses.h"
#include "ClassController.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>

DialogEditBuses::DialogEditBuses(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditBuses)
{
    ui->setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("editBusesGeometry").toByteArray());

    // Read all nets and buses and separate nets from buses
    ClassNetlist &Net = ::controller.getNetlist();
    for (auto &name : Net.getNetnames())
    {
        if (Net.get(name)) // Non-zero net number is a net
            ui->listNets->addItem(name);
        else // Zero net number is a bus
        {
            const QVector<net_t> bus = Net.getBus(name);
            QStringList nets;
            for (auto n : bus)
                nets.append(Net.get(n));
            add(name, nets);
        }
    }

    connect(ui->btCreate, &QPushButton::clicked, this, &DialogEditBuses::onCreate);
    connect(ui->btDelete, &QPushButton::clicked, this, &DialogEditBuses::onDelete);
    connect(ui->listBuses, &QListWidget::itemSelectionChanged, this, &DialogEditBuses::busSelChanged);
    connect(ui->listNets, &QListWidget::itemSelectionChanged, this, &DialogEditBuses::netSelChanged);
}

DialogEditBuses::~DialogEditBuses()
{
    QSettings settings;
    settings.setValue("editBusesGeometry", saveGeometry());

    delete ui;
}

/*
 * We are processing our own accept signal, sent when user clicks on the OK button
 * All buses are also added to the watchlist
 */
void DialogEditBuses::accept()
{
    ClassNetlist &Net = ::controller.getNetlist();

    // Sync up the watchlist to the updated class net buses
    QStringList watchlist = ::controller.getWatch().getWatchlist();

    // Rebuild all buses at the netlist class
    Net.clearBuses(); // from scratch
    for (int i=0; i<ui->listBuses->count(); i++)
    {
        QListWidgetItem *it = ui->listBuses->item(i);
        QStringList nets = it->toolTip().split(',');
        Net.addBus(it->text(), nets);

        // By default, add all buses to the watchlist
        watchlist.append(it->text());
    }
    ::controller.getWatch().updateWatchlist(watchlist);

    QDialog::done(QDialog::Accepted);
}

/*
 * Use net listbox selection changed signal to enable or disable Create button
 */
void DialogEditBuses::netSelChanged()
{
    QVector<QListWidgetItem *> sel = ui->listNets->selectedItems().toVector();
    ui->btCreate->setEnabled(sel.size() >= 2); // A bus needs to have at least 2 nets
}

/*
 * Use bus listbox selection changed signal to enable or disable Delete button
 */
void DialogEditBuses::busSelChanged()
{
    QVector<QListWidgetItem *> sel = ui->listBuses->selectedItems().toVector();
    if (sel.size())
        ui->labelNets->setText(sel[0]->toolTip());
    else
        ui->labelNets->clear();
    ui->btDelete->setEnabled(sel.size() == 1);
}

void DialogEditBuses::add(QString busName, QStringList nets)
{
    QListWidgetItem *li = new QListWidgetItem(busName);
    li->setToolTip(nets.join(','));
    ui->listBuses->addItem(li);
}

void DialogEditBuses::onCreate()
{
    QStringList nets;
    for (auto &net : ui->listNets->selectedItems())
        nets.append(net->text());
    if (nets.count() > 16) // Hardcoded maximum bus width
    {
        QMessageBox::critical(this, "Create a bus", "A bus cannot be wider than 16 nets!");
        return;
    }
    bool ok;
    QString name = QInputDialog::getText(this, "Create a bus", "Enter the bus name for a group of these nets:\n" + nets.join(','),
                                         QLineEdit::Normal, "", &ok, Qt::MSWindowsFixedSizeDialogHint);
    if (ok && name.trimmed().length() > 0)
    {
        name = name.trimmed().toUpper(); // Bus names are always upper-cased
        name = name.replace(' ', '_');
        if (!name.at(0).isLetter()) // Make sure the bus name starts with (an uppercased) letter
            name.prepend('B');
        if (ui->listBuses->findItems(name, Qt::MatchExactly).count() == 0) // Don't add it if a bus name already exists
            add(name, nets);
        else
            QMessageBox::critical(this, "Create a bus", "Bus with the name " + name + " already exists!", QMessageBox::Ok);
    }
}

void DialogEditBuses::onDelete()
{
    QVector<QListWidgetItem *> sel = ui->listBuses->selectedItems().toVector();
    Q_ASSERT(sel.size() == 1);
    int row = ui->listBuses->row(sel[0]);
    delete ui->listBuses->takeItem(row);
}
