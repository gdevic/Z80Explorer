#include "DialogSchematic.h"
#include "ui_DialogSchematic.h"
#include "ClassController.h"
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
        setWindowTitle(QString("Schematic for net %1 \"%2\"").arg(net).arg(name));

    m_scene = new QGraphicsScene(this);

    // Build the context menu that the schematic objects children will use
    m_menu = new QMenu();
    QAction *actionShow = new QAction("Show", this);
    QAction *actionNew = new QAction("Schematic...", this);
    m_menu->addAction(actionShow);
    m_menu->addAction(actionNew);

    // Define menu handlers
    // Note: The very first menu item should be "Show" since it is hard-coded in SymbolItem::mouseDoubleClickEvent
    connect(actionShow, &QAction::triggered, this, [=]()
    {
        SymbolItem *item = qgraphicsitem_cast<SymbolItem *>(m_scene->selectedItems().first());
        Q_ASSERT(item);
        doShow(item->get());
    });

    connect(actionNew, &QAction::triggered, this, [=]()
    {
        SymbolItem *item = qgraphicsitem_cast<SymbolItem *>(m_scene->selectedItems().first());
        Q_ASSERT(item);
        doNewSchematic(item->getNet());
    });

    QTimer::singleShot(0, this, &DialogSchematic::createDrawing);
}

DialogSchematic::~DialogSchematic()
{
    Logic::purge(m_logic);
    delete ui;
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
    int lastY;

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
    // XXX Need to find out how to set an adequate top-left margin
    m_scene->addRect(QRectF(-20,-50,0,0), QPen(Qt::white));
    ui->view->setScene(m_scene);
    ui->view->ensureVisible(QRect());
}
