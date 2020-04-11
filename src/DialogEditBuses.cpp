#include "DialogEditBuses.h"
#include "ui_DialogEditBuses.h"
#include "ClassController.h"
#include <QInputDialog>

DialogEditBuses::DialogEditBuses(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditBuses)
{
    ui->setupUi(this);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    // Read all nets and buses and separate nets from buses
    ClassNetlist &Net = ::controller.getNetlist();
    for (auto name : Net.getNodenames())
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
    connect(ui->listBuses, SIGNAL(itemSelectionChanged()), this, SLOT(busSelChanged()));
    connect(ui->listNets, SIGNAL(itemSelectionChanged()), this, SLOT(netSelChanged()));
}

DialogEditBuses::~DialogEditBuses()
{
    delete ui;
}

/*
 * We are processing our own accept signal, sent when user clicks on the OK button
 */
void DialogEditBuses::accept()
{
    ClassNetlist &Net = ::controller.getNetlist();
    // Rebuild all buses at the netlist class
    Net.clearBuses(); // from scratch
    for (int i=0; i<ui->listBuses->count(); i++)
    {
        QListWidgetItem *it = ui->listBuses->item(i);
        QStringList nets = it->toolTip().split(',');
        Net.addBus(it->text(), nets);
    }

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
    for (auto net : ui->listNets->selectedItems())
        nets.append(net->text());
    QString name = QInputDialog::getText(this, "Define a bus", "Enter the bus name for a group of these nets:\n" + nets.join(','), QLineEdit::Normal);
    if (!name.isNull() && name.trimmed().length() > 0)
    {
        name = name.trimmed().toUpper(); // Bus names are always upper-cased
        name = name.replace(' ', '_'); // YYY Should we be doing more checks on valid characters?
        if (ui->listBuses->findItems(name, Qt::MatchExactly).count() == 0) // Don't add it if a bus name already exists
            add(name, nets);
    }
}

void DialogEditBuses::onDelete()
{
    QVector<QListWidgetItem *> sel = ui->listBuses->selectedItems().toVector();
    Q_ASSERT(sel.size() == 1);
    int row = ui->listBuses->row(sel[0]);
    delete ui->listBuses->takeItem(row);
}