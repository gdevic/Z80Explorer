#include "DockWaveform.h"
#include "ui_DockWaveform.h"
#include "ClassController.h"
#include "DialogEditWatchlist.h"
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>

// Serialization support
#include "cereal/archives/binary.hpp"
#include "cereal/types/QString.hpp"
#include "cereal/types/QVector.hpp"
#include <fstream>

DockWaveform::DockWaveform(QWidget *parent) : QDockWidget(parent),
    ui(new Ui::DockWaveform)
{
    ui->setupUi(this);
    ui->widgetWaveform->setDock(this);

    // Build the menus for this widget
    QMenu* menu = new QMenu(this);
    menu->addAction("Load View...", this, SLOT(onLoad()));
    menu->addAction("Save View As...", this, SLOT(onSaveAs()));
    menu->addAction("Save View", this, SLOT(onSave()));
    ui->btFile->setMenu(menu);

    connect(ui->btEdit, &QToolButton::clicked, this, &DockWaveform::onEdit);
    connect(ui->btUp, &QToolButton::clicked, this, &DockWaveform::onUp);
    connect(ui->btDown, &QToolButton::clicked, this, &DockWaveform::onDown);

    rebuildList();
}

DockWaveform::~DockWaveform()
{
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
    QString fileName = QFileDialog::getOpenFileName(this, "Select a viewlist file to load", m_defName, "viewlist (*.vl);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (load(fileName))
            m_defName = fileName;
        else
            QMessageBox::critical(this, "Error", "Selected file is not a valid viewlist file");
        rebuildList();
    }
}

void DockWaveform::onSaveAs()
{
    // Prompts the user to select the viewlist file to save to
    QString fileName = QFileDialog::getSaveFileName(this, "Save viewlist to a file", m_defName, "viewlist (*.vl);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (save(fileName))
            m_defName = fileName;
        else
            QMessageBox::critical(this, "Error", "Unable to save viewlist to " + fileName);
    }
}

void DockWaveform::onSave()
{
    if (m_defName.isEmpty())
        return onSaveAs();
    if (!save(m_defName))
        QMessageBox::critical(this, "Error", "Unable to save viewlist to " + m_defName);
}

/*
 * Edits a collection of view items
 */
void DockWaveform::onEdit()
{
    // Let the user pick and select which nets are to be part of this view
    DialogEditWatchlist dlg(this);
    dlg.setNodeList(::controller.getWatch().getWatchlist());
    dlg.setWatchlist(getNames());
    if (dlg.exec()==QDialog::Accepted)
    {
        updateViewitems(dlg.getWatchlist());
        rebuildList();
    }
}

void DockWaveform::onUp()
{
}

void DockWaveform::onDown()
{
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

void DockWaveform::rebuildList()
{
    ui->list->clear();
    ui->list->addItems(getNames());
    ui->frame->update();
}
