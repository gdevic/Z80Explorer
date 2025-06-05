#include "ClassController.h"
#include "DialogEditWaveform.h"
#include "DockWaveform.h"
#include "ui_DockWaveform.h"
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QMutableVectorIterator>
#include <QScrollBar>
#include <QSettings>
#include <QStringBuilder>

DockWaveform::DockWaveform(QWidget *parent, QString sid) : QDockWidget(parent), ui(new Ui::DockWaveform)
{
    ui->setupUi(this);
    setWhatsThis(sid);
    setWindowTitle("Waveform " + sid);
    ui->widgetWaveform->init(this, sid);
    ui->btLink->setMinimumSize(ui->btEdit->sizeHint().width(), 0); // Tie the toolbutton width to btEdit's width so it's not too narrow
    ui->btDecorated->setMinimumSize(ui->btEdit->sizeHint().width(), 0); // Tie the toolbutton width to btEdit's width so it's not too narrow
    QSettings settings;
    restoreGeometry(settings.value("dockWaveformGeometry-" + sid).toByteArray());
    m_sectionSize = settings.value("dockWaveHeight-" + sid, 20).toInt();
    onEnlarge(0);

    // Build the menus for this widget
    QMenu *menu = new QMenu(this);
    menu->addAction("Load...", this, [=]() { onLoad(false); });
    menu->addAction("Merge...", this, [=]() { onLoad(true); });
    menu->addAction("Save As...", this, SLOT(onSaveAs()));
    menu->addAction("Export PNG...", this, SLOT(onPng()));
    ui->btFile->setMenu(menu);

    connect(ui->btEdit, &QToolButton::clicked, this, &DockWaveform::onEdit);
    connect(ui->btLink, &QToolButton::toggled, ui->widgetWaveform, &WidgetWaveform::onLinked);
    connect(ui->btDecorated, &QToolButton::toggled, ui->widgetWaveform, &WidgetWaveform::onDecorated);
    connect(ui->scrollArea->horizontalScrollBar(), &QAbstractSlider::rangeChanged, this, &DockWaveform::onScrollBarRangeChanged);
    connect(ui->scrollArea->horizontalScrollBar(), &QAbstractSlider::actionTriggered, this, &DockWaveform::onScrollBarActionTriggered);
    connect(ui->scrollArea, &CustomScrollArea::zoom, ui->widgetWaveform, &WidgetWaveform::onZoom);
    connect(ui->scrollArea, &CustomScrollArea::enlarge, ui->widgetWaveform, &WidgetWaveform::onEnlarge);
    connect(ui->scrollArea, &CustomScrollArea::enlarge, this, &DockWaveform::onEnlarge);
    connect(ui->containerScroll->verticalScrollBar(), &QScrollBar::sliderReleased, this, [=]() { emit verticalScrollStopped(); });
    connect(ui->widgetWaveform, &WidgetWaveform::cursorChanged, this, &DockWaveform::cursorChanged);
    connect(ui->widgetWaveform, &WidgetWaveform::scroll, this, &DockWaveform::scroll);
    connect(ui->widgetWaveform, &WidgetWaveform::setLink, this, [this](int value) { ui->btLink->setText(QString("Δ=%1").arg(value)); });
    connect(ui->widgetWaveform, &WidgetWaveform::cursorPosChanged, this, [=](uint index, uint pos)
        { if (ui->btSync->isChecked()) emit ::controller.syncWaveformCursorPos(sid, index, pos); });
    connect(&::controller, &ClassController::syncWaveformCursorPos, this, [=](QString sid, uint index, uint pos)
        { if (ui->btSync->isChecked() && (sid != whatsThis())) syncScroll(index, pos); });
    connect(&::controller, &ClassController::eventNetName, this, &DockWaveform::eventNetName);

    // Load default viewlist for this window id
    QString resDir = settings.value("ResourceDir").toString();
    m_fileViewlist = settings.value("waveform-" + sid, resDir + "/waveform-" + sid + ".json").toString();
    load(m_fileViewlist, false);

    rebuildList();
}

DockWaveform::~DockWaveform()
{
    Q_ASSERT(!m_fileViewlist.isEmpty());
    if (m_view.count()) // Save the configuration only if it is not empty
        save(m_fileViewlist);

    QSettings settings;
    settings.setValue("dockWaveformGeometry-" + whatsThis(), saveGeometry());
    settings.setValue("dockWaveHeight-" + whatsThis(), m_sectionSize);

    delete ui;
}

void DockWaveform::onLoad(bool merge)
{
    // Prompts the user to select which waveform view configuration file to load
    QString fileName = QFileDialog::getOpenFileName(this, "Select a waveform view configuration file to load", "", "waveform (*.json);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!load(fileName, merge))
            QMessageBox::critical(this, "Error", "Selected file is not a valid viewlist file");
        rebuildList();
    }
}

void DockWaveform::onSaveAs()
{
    // Prompts the user to select the waveform view configuration file to save to
    QString fileName = QFileDialog::getSaveFileName(this, "Save waveform view configuration to a file", "", "waveform (*.json);;All files (*.*)");
    if (!fileName.isEmpty())
    {
        if (!save(fileName))
            QMessageBox::critical(this, "Error", "Unable to save waveform view configuration to " + fileName);
    }
}

void DockWaveform::onPng()
{
    QPixmap pixmap = this->grab(QRect(QPoint(0, 0), this->size()));

    QString fileName = QFileDialog::getSaveFileName(this, "Save waveform as image", "", "PNG file (*.png);;All files (*.*)");
    if (!fileName.isEmpty() && !pixmap.toImage().save(fileName))
        QMessageBox::critical(this, "Error", "Unable to save image file " + fileName);
}

/*
 * Edits a collection of view items
 */
void DockWaveform::onEdit()
{
    DialogEditWaveform dlg(this, m_view);
    if (dlg.exec() == QDialog::Accepted)
    {
        dlg.getList(m_view);
        rebuildList();
    }
}

QStringList DockWaveform::getNames()
{
    QStringList items;
    for (auto &i : m_view)
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

viewitem *DockWaveform::find(net_t net)
{
    for (auto &i : m_view)
        if (i.net == net)
            return &i;
    return nullptr;
}

void DockWaveform::add(QString name)
{
    m_view.append(viewitem{ name, ::controller.getNetlist().get(name) });
}

/*
 * Handles messages about net name changes
 */
void DockWaveform::eventNetName(Netop op, const QString name, const net_t net)
{
    for (auto &i : m_view)
    {
        if (i.net == net)
        {
            switch (op)
            {
                case Netop::SetName: i.name = name; break;
                case Netop::Rename: i.name = name; break;
                case Netop::DeleteName: i.name = QString("(%1)").arg(net); break;
                case Netop::Changed: rebuildList(); break;
            }
        }
    }
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
    for (auto &name : items)
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
    for (int row = 0; row < m_view.count(); row++)
    {
        QTableWidgetItem *tvi = new QTableWidgetItem(m_view[row].name);
        tv->setItem(row, 0, tvi);
        tvi = new QTableWidgetItem("()");
        tv->setItem(row, 1, tvi);
    }

    // Update frame and scroll areas after content changes
    ui->frame->updateGeometry();
    ui->containerScroll->updateGeometry();
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
    for (int row = 0; row < m_view.count(); row++)
    {
        QTableWidgetItem *tvi = tv->item(row, 1);
        watch *w = ::controller.getWatch().find(m_view[row].name);
        pin_t data_cur = ::controller.getWatch().at(w, hcycle);

        if (data_cur < 2)
            tvi->setText(QString::number(data_cur));
        else if (data_cur == 2)
            tvi->setText("hi-Z");
        else if (data_cur == 3)
            tvi->setText("no-data");
        else if (data_cur == 4) // Bus
        {
            uint width, value = ::controller.getWatch().at(w, hcycle, width);
            if (width)
            {
                // For Disasm format, print the hex opcode instead
                if (m_view[row].format == ClassController::FormatBus::Disasm)
                    tvi->setText(::controller.formatBus(ClassController::FormatBus::Hex, value, width));
                else
                    tvi->setText(::controller.formatBus(m_view[row].format, value, width));
            }
            else
                tvi->setText("no-data");
        }
        else
            tvi->setText("error");
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
 * Synchronize the horizontal scroll with the cursor. This is used with the "Sync" option.
 */
void DockWaveform::syncScroll(uint index, uint pos)
{
    qreal scale = ui->widgetWaveform->setCursorsPos(index, pos);
    CustomScrollArea *sb = ui->scrollArea;
    sb->ensureVisible(pos * scale / 2, 50, 50);
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
 * User moved the horizontal scroll bar on the waveform pane
 */
void DockWaveform::onScrollBarActionTriggered(int)
{
    QScrollBar *sb = ui->scrollArea->horizontalScrollBar();
    uint range = sb->maximum();
    uint pos = sb->sliderPosition();
    m_rel = qreal(pos) / range;
}

/*
 * Handles mouse wheel event in the scroll area to support vertical resize
 */
void DockWaveform::wheelEvent(QWheelEvent *event)
{
    bool ctrl = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
    if (ctrl)
        emit ui->scrollArea->enlarge(event->angleDelta().y() > 0 ? 1 : -1);
}

/*
 * User resized the view vertically
 */
void DockWaveform::onEnlarge(int delta)
{
    m_sectionSize = qBound<int>(10, m_sectionSize + delta, 50);
    ui->list->verticalHeader()->setDefaultSectionSize(m_sectionSize);
    QFont font = ui->list->font();
    font.setPixelSize(0.7 * m_sectionSize);
    ui->list->setFont(font);

    // Update scroll area size hint and container scroll area to reflect new content height
    ui->frame->updateGeometry();
    ui->containerScroll->updateGeometry();
}

/*
 * Loads waveform view configuration from a file
 */
bool DockWaveform::load(QString fileName, bool merge)
{
    qInfo() << "Loading waveform view configuration from" << fileName;
    QFile loadFile(fileName);
    if (loadFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(data));
        const QJsonObject json = loadDoc.object();

        if (json.contains("waveform") && json["waveform"].isArray())
        {
            QJsonArray array = json["waveform"].toArray();
            if (!merge)
            {
                m_view.clear();
                m_view.reserve(array.size());
            }

            for (int i = 0; i < array.size(); i++)
            {
                viewitem a;
                QJsonObject obj = array[i].toObject();
                if (obj.contains("name") && obj["name"].isString())
                    a.name = obj["name"].toString();
                if (obj.contains("net") && obj["net"].isDouble())
                    a.net = obj["net"].toInt();
                if (obj.contains("format") && obj["format"].isDouble())
                    a.format = obj["format"].toInt();
                if (obj.contains("color") && obj["color"].isString())
                {
                    QStringList s = obj["color"].toString().split(',');
                    if (s.count() == 4)
                        a.color = QColor(s[0].toInt(), s[1].toInt(), s[2].toUInt(), s[3].toInt());
                }
                // Make sure the net or bus has already been named and is valid
                if (::controller.getNetlist().verifyNetBus(a.name, a.net) == false)
                {
                    qWarning() << "Unmatched net/bus name" << a.name << "(" << a.net << ") in the waveform config .. Skipping.";
                    continue;
                }
                m_view.append(a);
            }
            return true;
        }
        else
            qWarning() << "Invalid json file";
    }
    else
        qWarning() << "Unable to load" << fileName;
    return false;
}

/*
 * Saves waveform view configuration to a file
 * Note: Saving of waveform items function is duplicated in DialogEditWaveform.cpp where we save only selected items
 */
bool DockWaveform::save(QString fileName)
{
    qInfo() << "Saving waveform view configuration to" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
    {
        QJsonObject json;
        QJsonArray jsonArray;
        for (viewitem &a : m_view)
        {
            QJsonObject obj;
            obj["name"] = a.name;
            obj["net"] = a.net;
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
