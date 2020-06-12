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
    setAttribute(Qt::WA_DeleteOnClose, false); // We never delete this view; we just hide it

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
    delete ui;
}

/*
 * Recursively draws symbols
 */
void DialogSchematic::drawSymbol(QPointF loc, Logic *lr)
{
    SymbolItem *l0 = new SymbolItem(lr);
    m_scene->addItem(l0);
    l0->setPos(loc);

    for (auto i=0; i < lr->children.count(); i++)
        drawSymbol(loc + QPointF(50, i * 50), lr->children[i]);
}

/*
 * Pre-builds the tree to calculate screen positions by adding the sizes of child nodes
 */
int DialogSchematic::preBuild(Logic *lr)
{
    for (auto k : lr->children)
        lr->tag += preBuild(k);
    return lr->tag;
}

void DialogSchematic::createDrawing()
{
    preBuild(m_logic);
    drawSymbol(QPointF(0,0), m_logic);
    ui->view->setScene(m_scene);
}
