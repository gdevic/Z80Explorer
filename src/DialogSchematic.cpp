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

    QTimer::singleShot(0, this, &DialogSchematic::createDrawing);
}

DialogSchematic::~DialogSchematic()
{
    Logic::purge(m_logic);
    delete ui;
}

/*
 * Recursively draws symbols in a tree structure
 */
void DialogSchematic::drawSymbol(QPointF loc, Logic *lr)
{
    SymbolItem *l0 = new SymbolItem(lr);
    m_scene->addItem(l0);
    l0->setPos(loc);

    QPointF childLoc(loc.x() + 50 + 10, loc.y());
    for (auto &k : lr->children)
    {
        drawSymbol(childLoc, k);
        childLoc += QPointF(0, k->tag * 50);
    }
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
    drawSymbol(QPointF(0, 0), m_logic);
    ui->view->setScene(m_scene);
    ui->view->ensureVisible(QRect());
}
