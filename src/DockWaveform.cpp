#include "DockWaveform.h"
#include "ui_DockWaveform.h"
#include "ClassController.h"
#include "DialogEditWaveform.h"
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStringBuilder>

// Serialization support
#include "cereal/archives/binary.hpp"
#include "cereal/types/QString.hpp"
#include "cereal/types/QVector.hpp"
#include <fstream>

DockWaveform::DockWaveform(QWidget *parent, uint id) : QDockWidget(parent),
    ui(new Ui::DockWaveform)
{
    ui->setupUi(this);
    setWindowTitle("Waveform-" + QString::number(id));
    ui->widgetWaveform->setDock(this);

    // Build the menus for this widget
    QMenu* menu = new QMenu(this);
    menu->addAction("Load View...", this, SLOT(onLoad()));
    menu->addAction("Save View As...", this, SLOT(onSaveAs()));
    menu->addAction("Save View", this, SLOT(onSave()));
    ui->btFile->setMenu(menu);

    connect(ui->btEdit, &QToolButton::clicked, this, &DockWaveform::onEdit);
    connect(ui->widgetWaveform, SIGNAL(cursorChanged(uint)), this, SLOT(cursorChanged(uint)));

    // Load default viewlist for this window id
    QSettings settings;
    QString resDir = settings.value("ResourceDir").toString();
    Q_ASSERT(!resDir.isEmpty());
    m_fileViewlist = settings.value("ViewlistFile-" + QString::number(id), resDir + "/viewlist-" + QString::number(id) + ".vl").toString();
    load(m_fileViewlist);

    rebuildList();
}

DockWaveform::~DockWaveform()
{
    Q_ASSERT(!m_fileViewlist.isEmpty());
    if (m_view.count()) // Save the view only if it is not empty
        save(m_fileViewlist);
    delete ui;
}

/*
 * Loads view items
 */
bool DockWaveform::load(QString fileName)
{
    try
    {
        std::ifstream os(fileName.toLatin1(), std::ios::binary);
        cereal::BinaryInputArchive archive(os);
        archive(m_view);
    }
    catch(...) { qWarning() << "Unable to load" << fileName; }
    return true;
}

/*
 * Saves current view items
 */
bool DockWaveform::save(QString fileName)
{
    try
    {
        std::ofstream os(fileName.toLatin1(), std::ios::binary);
        cereal::BinaryOutputArchive archive(os);
        archive(m_view);
    }
    catch(...) { qWarning() << "Unable to save" << fileName; }
    return true;
}

void DockWaveform::onLoad()
{
    // Prompts the user to select which viewlist file to load
    QString fileName = QFileDialog::getOpenFileName(this, "Select a viewlist file to load", "", "viewlist (*.vl);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!load(fileName))
            QMessageBox::critical(this, "Error", "Selected file is not a valid viewlist file");
        rebuildList();
    }
}

void DockWaveform::onSaveAs()
{
    // Prompts the user to select the viewlist file to save to
    QString fileName = QFileDialog::getSaveFileName(this, "Save viewlist to a file", "", "viewlist (*.vl);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!save(fileName))
            QMessageBox::critical(this, "Error", "Unable to save viewlist to " + fileName);
    }
}

void DockWaveform::onSave()
{
    Q_ASSERT(!m_fileViewlist.isEmpty());
    if (!save(m_fileViewlist))
        QMessageBox::critical(this, "Error", "Unable to save viewlist to " + m_fileViewlist);
}

/*
 * Edits a collection of view items
 */
void DockWaveform::onEdit()
{
    DialogEditWaveform dlg(this, m_view);
    if (dlg.exec()==QDialog::Accepted)
    {
        m_view = dlg.get();
        rebuildList();
    }
}

QStringList DockWaveform::getNames()
{
    QStringList items;
    for (auto i : m_view)
        items.append(i.name);
    return items;
}

viewitem *DockWaveform::find(QString name)
{
    for (auto &i : m_view)
        if (i.name == name)
            return &i;
    return nullptr;
}

void DockWaveform::add(QString name)
{
    m_view.append(viewitem { name });
}

/*
 * Updates the view item names: adds new items not on our list and removes those not on the items
 * For the items that are unchanged, preserve the order in the list
 */
void DockWaveform::updateViewitems(QStringList items)
{
    // 1. Remove items present in m_view but not present in items
    QMutableVectorIterator<viewitem> it(m_view);
    while (it.hasNext())
        if (!items.contains(it.next().name))
            it.remove();
    // 2. Add items present in items but not present in m_view
    for (auto name : items)
        if (find(name) == nullptr)
            add(name);
}

/*
 * Cleans and rebuilds the list of view items based on the current, presumably updated, m_view
 */
void DockWaveform::rebuildList()
{
    QTableWidget *tv = ui->list;
    tv->clearContents();
    tv->setRowCount(m_view.count());
    tv->setColumnCount(2);
    for (int row=0; row < m_view.count(); row++)
    {
        QTableWidgetItem *tvi = new QTableWidgetItem(m_view[row].name);
        tv->setItem(row, 0, tvi);
        tvi = new QTableWidgetItem("()");
        tv->setItem(row, 1, tvi);
    }
    ui->frame->update();
}

/*
 * Cursor on the right page changed position and we need to update net and bus values
 */
void DockWaveform::cursorChanged(uint hcycle)
{
    if (m_lastcursor == hcycle)
        return;
    m_lastcursor = hcycle;

    QTableWidget *tv = ui->list;
    for (int row=0; row < m_view.count(); row++)
    {
        watch *w = ::controller.getWatch().find(m_view[row].name);
        pin_t data_cur = ::controller.getWatch().at(w, hcycle);

        QString display = QString::number(data_cur);
        if (data_cur == 4) // Bus
        {
            uint width;
            uint bus = ::controller.getWatch().at(w, hcycle, width);
            if (width)
                display = QString::number(width) % "'h" % QString::number(bus, 16);
            else
                display = "?";
        }

        QTableWidgetItem *tvi = tv->item(row, 1);
        tvi->setText(display);
    }
}
