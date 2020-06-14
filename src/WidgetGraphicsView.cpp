#include "WidgetGraphicsView.h"
#include <QDebug>
#include <QPainterPath>
#include <QWheelEvent>

WidgetGraphicsView::WidgetGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
}

/*
 * Handle mouse wheel event to zoom in and out
 */
void WidgetGraphicsView::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0)
        m_scale = qMin(3.0, m_scale * 1.1);
    else
        m_scale = qMax(0.2, m_scale / 1.1);

    QTransform oldMatrix = transform();
    resetTransform();
    translate(oldMatrix.dx(), oldMatrix.dy());
    scale(m_scale, m_scale);
}

/*
 * Constructor creates logic symbols for the logic type it is constructed
 */
SymbolItem::SymbolItem(Logic *lr, QGraphicsItem *parent) :
    QGraphicsPolygonItem(parent),
    m_lr(lr)
{
    QPainterPath path;

    // Pre-build basic diagram shapes for each logic symbol
    switch (lr->op)
    {
        case LogicOp::Nop:
            path.lineTo(10, 10);
            path.lineTo(50, 10);
            path.lineTo(50,-10);
            path.lineTo(10,-10);
            break;
        case LogicOp::Inverter:
            path.lineTo(10, 0);
            path.arcTo(QRectF(10, -5, 10, 10), 180, -180);
            path.lineTo(50.0, -20.0);
            path.lineTo(50.0, 20.0);
            path.lineTo(20.0,  0.0);
            path.arcTo(QRectF(10, -5, 10, 10), 0, -180);
            break;
        case LogicOp::And:
            path.lineTo(10, 0);
            path.arcTo(QRectF(10, -25, 50, 50), 180, -90);
            path.lineTo(50, -25);
            path.lineTo(50, 25);
            path.arcTo(QRectF(10, -25, 50, 50), -90, -90);
            break;
        case LogicOp::Nand:
            path.moveTo(10, 0);
            path.arcTo(0, -5, 10, 10, 0, 360);
            path.arcTo(QRectF(10, -25, 50, 50), 180, -90);
            path.lineTo(50, -25);
            path.lineTo(50, 25);
            path.arcTo(QRectF(10, -25, 50, 50), -90, -90);
            break;
        case LogicOp::Or:
            path.lineTo(10, 0);
            path.arcTo(QRectF(10, -25, 50, 50), 180, -90);
            path.lineTo(50, -25);
            path.arcTo(QRectF(40, -35, 70, 70), 140, 86);
            path.arcTo(QRectF(10, -25, 50, 50), -90, -90);
            break;
        case LogicOp::Nor:
            path.moveTo(10, 0);
            path.arcTo(0, -5, 10, 10, 0, 360);
            path.arcTo(QRectF(10, -25, 50, 50), 180, -90);
            path.lineTo(50, -25);
            path.arcTo(QRectF(40, -35, 70, 70), 140, 86);
            path.arcTo(QRectF(10, -25, 50, 50), -90, -90);
            break;
    }
    m_poly = path.toFillPolygon();

    setPolygon(m_poly);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    // Items are not movable but this is useful to keep for debug
    //setFlag(QGraphicsItem::ItemIsMovable, true);
}

/*
 * Do additional painting on each symbol
 */
void SymbolItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QString type;
    switch (m_lr->op)
    {
        case LogicOp::Nop: type = ""; break;
        case LogicOp::Inverter: type = "INV"; break;
        case LogicOp::And: type = "AND"; break;
        case LogicOp::Nand: type = "NAND"; break;
        case LogicOp::Or: type = "OR"; break;
        case LogicOp::Nor: type = "NOR"; break;
    }

    QRect r(0, -25, 48, 50);
    painter->drawText(r, Qt::AlignVCenter | Qt::AlignRight, m_lr->name);
    painter->drawText(r, Qt::AlignBottom, type);

    QGraphicsPolygonItem::paint(painter, option, widget);
}
