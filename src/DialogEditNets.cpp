#include "DialogEditNets.h"
#include "ui_DialogEditNets.h"
#include "ClassController.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>

DialogEditNets::DialogEditNets(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogEditNets)
{
    ui->setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("editNetsGeometry").toByteArray());

    // Read all nets and buses and separate nets from buses
    ClassNetlist &Net = ::controller.getNetlist();
    for (auto &name : Net.getNetnames())
    {
        if (Net.get(name)) // Non-zero net number is a net
            ui->listNets->addItem(name);
    }

    connect(ui->btDelete, &QPushButton::clicked, this, &DialogEditNets::onDelete);
    connect(ui->btRename, &QPushButton::clicked, this, &DialogEditNets::onRename);
    connect(ui->listNets, &QListWidget::itemSelectionChanged, this, &DialogEditNets::netSelChanged);
}

DialogEditNets::~DialogEditNets()
{
    QSettings settings;
    settings.setValue("editNetsGeometry", saveGeometry());

    delete ui;
}

/*
 * Use net listbox selection changed signal to enable or disable buttons
 */
void DialogEditNets::netSelChanged()
{
    QVector<QListWidgetItem *> sel = ui->listNets->selectedItems().toVector();
    ui->btDelete->setEnabled(sel.size());
    ui->btRename->setEnabled(sel.size() == 1); // Can rename only one net at a time
}

void DialogEditNets::onDelete()
{
    QVector<QListWidgetItem *> sel = ui->listNets->selectedItems().toVector();
    Q_ASSERT(sel.size());
    if ((sel.size() > 1) && QMessageBox::question(this, "Delete nets", "Are you sure you want to delete " + QString::number(sel.size()) + " net names?") != QMessageBox::Yes)
        return;
    for (auto i : sel)
    {
        net_t n = ::controller.getNetlist().get(i->text());
        ::controller.deleteNetName(n);
        delete ui->listNets->takeItem(ui->listNets->row(i));
    }
}

void DialogEditNets::onRename()
{
    QVector<QListWidgetItem *> sel = ui->listNets->selectedItems().toVector();
    Q_ASSERT(sel.size() == 1);
    net_t newNet = ::controller.getNetlist().get(sel[0]->text());
    QStringList allNames = ::controller.getNetlist().getNetnames();
    QString oldName = sel[0]->text();
    bool ok;
    QString newName = QInputDialog::getText(this, "Edit net name", "Enter the new name (alias) for the selected net " + QString::number(newNet) + "\n",
                                            QLineEdit::Normal, oldName, &ok, Qt::MSWindowsFixedSizeDialogHint);
    newName = newName.trimmed().toLower(); // Trim spaces and keep net names lowercased
    if (!ok || (newName == oldName))
        return;
    net_t otherNet = ::controller.getNetlist().get(newName);
    if (allNames.contains(newName))
    {
        if (QMessageBox::question(this, "Edit net name", "The name '" + newName + "' is already attached to another net.\nDo you want to continue (the other net will become nameless)?") != QMessageBox::Yes)
            return;
        ::controller.deleteNetName(otherNet);
    }
    if (newName.isEmpty())
    {
        if (QMessageBox::question(this, "Edit net name", "Delete name '" + oldName + "' of the net " + QString::number(newNet) + " ?") != QMessageBox::Yes)
            return;
        ::controller.deleteNetName(newNet);
        delete ui->listNets->takeItem(ui->listNets->row(sel[0]));
    }
    else
    {
        ::controller.renameNet(newName, newNet);
        sel[0]->setText(newName);
    }
}
