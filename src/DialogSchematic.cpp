#include "DialogSchematic.h"
#include "ui_DialogSchematic.h"
#include "ClassController.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStringBuilder>
#include <QTimer>

DialogSchematic::DialogSchematic(QWidget *parent, Logic *lr) :
    QDialog(parent),
    ui(new Ui::DialogSchematic),
    m_logic(lr)
{
    ui->setupUi(this);

    net_t net = lr->net;
    QString name = ::controller.getNetlist().get(net);
    if (name.isEmpty())
        setWindowTitle(QString("Schematic for net %1").arg(net));
    else
        setWindowTitle(QString("Schematic for net %1:%2").arg(name).arg(net));

    m_scene = new QGraphicsScene(this);

    // Build the context menu that the schematic objects children will use
    m_menu = new QMenu();
    QAction *actionShow = new QAction("Show", this);
    QAction *actionNew = new QAction("Schematic...", this);
    QAction *actionPng = new QAction("Export PNG...", this);
    m_menu->addAction(actionShow);
    m_menu->addAction(actionNew);
    m_menu->addAction(actionPng);

    // Note: The very first menu item should be "Show" since it is hard-coded in SymbolItem::mouseDoubleClickEvent
    connect(actionShow, &QAction::triggered, this, &DialogSchematic::onShow);
    connect(actionNew, &QAction::triggered, this, &DialogSchematic::onNewSchematic);
    connect(actionPng, &QAction::triggered, this, &DialogSchematic::onPng);

    QTimer::singleShot(0, this, &DialogSchematic::createDrawing);
}

DialogSchematic::~DialogSchematic()
{
    Logic::purge(m_logic);
    delete ui;
}

void DialogSchematic::onShow()
{
    SymbolItem *item = qgraphicsitem_cast<SymbolItem *>(m_scene->selectedItems().first());
    Q_ASSERT(item);
    doShow(item->get());
}

void DialogSchematic::onNewSchematic()
{
    SymbolItem *item = qgraphicsitem_cast<SymbolItem *>(m_scene->selectedItems().first());
    Q_ASSERT(item);
    doNewSchematic(item->getNet());
}

void DialogSchematic::onPng()
{
    m_scene->clearSelection();
    QImage image(m_scene->sceneRect().size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    m_scene->render(&painter);

    QString fileName = QFileDialog::getSaveFileName(this, "Save diagram as image", "", "PNG file (*.png);;All files (*.*)");
    if (!fileName.isEmpty() && !image.save(fileName))
        QMessageBox::critical(this, "Error", "Unable to save image file " + fileName);
}

/*
 * Recursively draws symbols and connecting lines of a logic tree structure
 */
void DialogSchematic::drawSymbol(QPoint loc, Logic *lr)
{
    SymbolItem *sym = new SymbolItem(lr, m_menu);
    m_scene->addItem(sym);
    sym->setPos(loc);

    QPoint childLoc(loc.x() + 50 + 10, loc.y());
    QPoint lineStart(loc.x() + 50 + 5, loc.y());
    QLine line(lineStart, childLoc);
    int lastY {};

    for (auto k : lr->children)
    {
        drawSymbol(childLoc, k);

        m_scene->addLine(line);
        lastY = childLoc.y();
        QPoint nextDown(0, k->tag * 50);
        childLoc += nextDown;
        line.translate(nextDown);
    }

    // Draw the horizontal line from the end of this node to the first child
    if (lr->children.count())
        m_scene->addLine(loc.x() + 50, loc.y(), lineStart.x(), loc.y());

    // Draw the vertical line connecting all the children
    if (lr->children.count() > 1)
        m_scene->addLine(QLine(lineStart, QPoint(lineStart.x(), lastY)));
}

/*
 * Pre-builds the nodes to calculate screen positions by adding the number of child nodes
 */
int DialogSchematic::preBuild(Logic *lr)
{
    if (lr->children.count() == 0)
        lr->tag = 1;
    for (auto k : lr->children)
        lr->tag += preBuild(k);
    return lr->tag;
}

void DialogSchematic::createDrawing()
{
    preBuild(m_logic);
    drawSymbol(QPoint(0, 0), m_logic);
    ui->view->setScene(m_scene);
    // Add some margins to the scene view
    m_scene->setSceneRect(m_scene->sceneRect().adjusted(-50, -50, 50, 50));
    ui->view->ensureVisible(QRect());
}
