#include "WidgetGraphicsView.h"
#include <QDebug>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QPainterPath>
#include <QWheelEvent>

WidgetGraphicsView::WidgetGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    setAttribute(Qt::WA_AcceptTouchEvents);
}

/*
 * Handle mouse wheel event to zoom in and out
 */
void WidgetGraphicsView::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0)
        m_scale = m_scale * 1.1;
    else
        m_scale = m_scale / 1.1;
    m_scale = qBound(0.2, m_scale, 3.0);
    setTransform(QTransform::fromScale(m_scale, m_scale));
}

/*
 * Handle pinch-to-zoom on touch displays
 */
bool WidgetGraphicsView::viewportEvent(QEvent *event)
{
    switch (event->type())
    {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
        if (touchPoints.count() == 2)
        {
            // Determine the current scale factor independently of m_scale
            const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
            const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();
            qreal currentScaleFactor =
                    QLineF(touchPoint0.pos(), touchPoint1.pos()).length() /
                    QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();
            if (touchEvent->touchPointStates() & Qt::TouchPointReleased)
            {
                m_scale *= currentScaleFactor;
                m_scale = qBound(0.2, m_scale, 3.0);
                currentScaleFactor = 1;
            }
            setTransform(QTransform::fromScale(qMin(m_scale * currentScaleFactor, 3.0),
                                               qMin(m_scale * currentScaleFactor, 3.0)));
        }
        return true;
    }
    default:
        break;
    }
    return QGraphicsView::viewportEvent(event);
}

/*
 * Constructor creates logic symbols for the logic type it is constructed
 */
SymbolItem::SymbolItem(Logic *lr, QMenu *menu, QGraphicsItem *parent) :
    QGraphicsPolygonItem(parent),
    m_lr(lr),
    m_menu(menu)
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
            path.arcTo(QRectF(40, -35, 70, 70), 140, 40);
            path.lineTo(50, 0);
            path.lineTo(40, 0);
            path.arcTo(QRectF(40, -35, 70, 70), 183, 43);
            path.arcTo(QRectF(10, -25, 50, 50), -90, -90);
            break;
        case LogicOp::Nor:
            path.moveTo(10, 0);
            path.arcTo(0, -5, 10, 10, 0, 360);
            path.arcTo(QRectF(10, -25, 50, 50), 180, -90);
            path.lineTo(50, -25);
            path.arcTo(QRectF(40, -35, 70, 70), 140, 40);
            path.lineTo(50, 0);
            path.lineTo(40, 0);
            path.arcTo(QRectF(40, -35, 70, 70), 183, 43);
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
 * Do additional painting on each symbol: draw the logic net name and the symbol function
 */
void SymbolItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QString type;
    int textRightEnd = 48; // Shift the text a little bit to the left for OR and NOR symbols
    switch (m_lr->op)
    {
        case LogicOp::Nop: type = ""; break;
        case LogicOp::Inverter: type = "INV"; break;
        case LogicOp::And: type = "AND"; break;
        case LogicOp::Nand: type = "NAND"; break;
        case LogicOp::Or: type = "OR", textRightEnd = 40; break;
        case LogicOp::Nor: type = "NOR", textRightEnd = 40; break;
    }

    QRect bounds(0, -25, textRightEnd, 50);
    painter->drawText(bounds, Qt::AlignVCenter | Qt::AlignRight, m_lr->name);
    painter->drawText(bounds, Qt::AlignBottom, type);

    QGraphicsPolygonItem::paint(painter, option, widget);
}

/*
 * Context menu handler, called when the user right-clicks on the symbol
 * The symbol does not own the menu; the menu is created and implemented in DialogSchematic class
 */
void SymbolItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // Make sure that only one - this symbol - is selected in the scene
    scene()->clearSelection();
    setSelected(true);
    m_menu->exec(event->screenPos());
}

void SymbolItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *)
{
    // Make sure that only one - this symbol - is selected in the scene
    scene()->clearSelection();
    setSelected(true);
    // Hard-coded action[0] to be the menu's "Show" action
    m_menu->actions()[0]->trigger();
}
