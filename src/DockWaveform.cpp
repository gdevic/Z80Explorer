#include "DockWaveform.h"
#include "ui_DockWaveform.h"
#include "ClassController.h"
#include "DialogEditWaveform.h"
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QStringBuilder>

DockWaveform::DockWaveform(QWidget *parent, uint id) : QDockWidget(parent),
    ui(new Ui::DockWaveform),
    m_id(id)
{
    ui->setupUi(this);
    setWindowTitle("Waveform " + QString::number(m_id));
    ui->widgetWaveform->setDock(this);
    QSettings settings;
    restoreGeometry(settings.value("dockWaveformGeometry-" + QString::number(m_id)).toByteArray());

    // Build the menus for this widget
    QMenu* menu = new QMenu(this);
    menu->addAction("Load View...", this, SLOT(onLoad()));
    menu->addAction("Save View As...", this, SLOT(onSaveAs()));
    menu->addAction("Save View", this, SLOT(onSave()));
    menu->addAction("Export PNG...", this, SLOT(onPng()));
    ui->btFile->setMenu(menu);

    connect(ui->btEdit, &QToolButton::clicked, this, &DockWaveform::onEdit);
    connect(ui->widgetWaveform, SIGNAL(cursorChanged(uint)), this, SLOT(cursorChanged(uint)));
    connect(ui->widgetWaveform, SIGNAL(scroll(int)), this, SLOT(scroll(int)));
    connect(ui->scrollArea->horizontalScrollBar(), &QAbstractSlider::rangeChanged, this, &DockWaveform::onScrollBarRangeChanged);
    connect(ui->scrollArea->horizontalScrollBar(), &QAbstractSlider::actionTriggered, this, &DockWaveform::onScrollBarActionTriggered);

    // Load default viewlist for this window id
    QString resDir = settings.value("ResourceDir").toString();
    Q_ASSERT(!resDir.isEmpty());
    m_fileViewlist = settings.value("waveform-" + QString::number(m_id), resDir + "/waveform-" + QString::number(id) + ".json").toString();
    load(m_fileViewlist);

    rebuildList();
}

DockWaveform::~DockWaveform()
{
    Q_ASSERT(!m_fileViewlist.isEmpty());
    if (m_view.count()) // Save the configuration only if it is not empty
        save(m_fileViewlist);

    QSettings settings;
    settings.setValue("dockWaveformGeometry-" + QString::number(m_id), saveGeometry());

    delete ui;
}

void DockWaveform::onLoad()
{
    // Prompts the user to select which waveform configuration file to load
    QString fileName = QFileDialog::getOpenFileName(this, "Select a waveform configuration file to load", "", "waveform (*.json);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!load(fileName))
            QMessageBox::critical(this, "Error", "Selected file is not a valid viewlist file");
        rebuildList();
    }
}

void DockWaveform::onSaveAs()
{
    // Prompts the user to select the waveform configuration file to save to
    QString fileName = QFileDialog::getSaveFileName(this, "Save waveform configuration to a file", "", "waveform (*.json);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!save(fileName))
            QMessageBox::critical(this, "Error", "Unable to save waveform configuration to " + fileName);
    }
}

void DockWaveform::onSave()
{
    Q_ASSERT(!m_fileViewlist.isEmpty());
    if (!save(m_fileViewlist))
        QMessageBox::critical(this, "Error", "Unable to save waveform configuration to " + m_fileViewlist);
}

void DockWaveform::onPng()
{
    QPixmap pixmap = this->grab(QRect(QPoint(0, 0), this->size()));

    QString fileName = QFileDialog::getSaveFileName(this, "Save waveform as image", "image.png", "png file (*.png);;All files (*.*)");
    if (!fileName.isEmpty() && !pixmap.toImage().save(fileName))
        QMessageBox::critical(this, "Error", "Unable to save image file " + fileName);
}

/*
 * Edits a collection of view items
 */
void DockWaveform::onEdit()
{
    DialogEditWaveform dlg(this, m_view);
    if (dlg.exec()==QDialog::Accepted)
    {
        dlg.getList(m_view);
        rebuildList();
    }
}

QStringList DockWaveform::getNames()
{
    QStringList items;
    for (const auto &i : m_view)
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
    for (const auto name : items)
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
        if (data_cur == 3)
            display = "no-data";
        else
        if (data_cur == 4) // Bus
        {
            uint width, value = ::controller.getWatch().at(w, hcycle, width);
            if (width)
                display = ::controller.formatBus(m_view[row].format, value, width);
            else
                display = "no-data";
        }

        QTableWidgetItem *tvi = tv->item(row, 1);
        tvi->setText(display);
    }
}

/*
 * User moved the view using a mouse, scroll it
 */
void DockWaveform::scroll(int deltaX)
{
    QScrollBar *sb = ui->scrollArea->horizontalScrollBar();
    qreal new_pos = sb->sliderPosition() + deltaX;
    sb->setSliderPosition(new_pos);
}

/*
 * User changed the scaling on the waveform and that caused a range change
 */
void DockWaveform::onScrollBarRangeChanged(int, int max)
{
    QScrollBar *sb = ui->scrollArea->horizontalScrollBar();
    qreal new_pos = m_rel * max;
    sb->setSliderPosition(new_pos);
}

/*
 * User managed the horizontal scroll bar on the waveform pane
 */
void DockWaveform::onScrollBarActionTriggered(int)
{
    QScrollBar *sb = ui->scrollArea->horizontalScrollBar();
    uint range = sb->maximum();
    uint pos = sb->sliderPosition();
    m_rel = qreal(pos) / range;
}

/*
 * Loads waveform configuration from a file
 */
bool DockWaveform::load(QString fileName)
{
    qInfo() << "Loading waveform configuration from" << fileName;
    QFile loadFile(fileName);
    if (loadFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(data));
        const QJsonObject json = loadDoc.object();

        if (json.contains("waveform") && json["waveform"].isArray())
        {
            QJsonArray array = json["waveform"].toArray();
            m_view.clear();
            m_view.reserve(array.size());

            for (int i = 0; i < array.size(); i++)
            {
                viewitem a;
                QJsonObject obj = array[i].toObject();
                if (obj.contains("name") && obj["name"].isString())
                    a.name = obj["name"].toString();
                if (obj.contains("format") && obj["format"].isDouble())
                    a.format = obj["format"].toInt();
                if (obj.contains("color") && obj["color"].isString())
                {
                    QStringList s = obj["color"].toString().split(',');
                    if (s.count() == 4)
                        a.color = QColor(s[0].toInt(), s[1].toInt(), s[2].toUInt(), s[3].toInt());
                }
                m_view.append(a);
            }
        }
        return true;
    }
    else
        qWarning() << "Unable to load" << fileName;
    return false;
}

/*
 * Saves waveform configuration to a file
 */
bool DockWaveform::save(QString fileName)
{
    qInfo() << "Saving waveform configuration to" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
    {
        QJsonObject json;
        QJsonArray jsonArray;
        for (const viewitem &a : m_view)
        {
            QJsonObject obj;
            obj["name"] = a.name;
            obj["format"] = int(a.format);
            obj["color"] = QString("%1,%2,%3,%4").arg(a.color.red()).arg(a.color.green()).arg(a.color.blue()).arg(a.color.alpha());
            jsonArray.append(obj);
        }
        json["waveform"] = jsonArray;

        QJsonDocument saveDoc(json);
        saveFile.write(saveDoc.toJson());
        return true;
    }
    else
        qWarning() << "Unable to save" << fileName;
    return false;
}
