#include "ClassController.h"
#include "WidgetGraphicsView.h"
#include <QDebug>
#include <QGraphicsSceneContextMenuEvent>
#include <QGuiApplication>
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
    if (event->angleDelta().y() > 0)
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
        QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->points();
        if (touchPoints.count() == 2)
        {
            // Determine the current scale factor independently of m_scale
            const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
            const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();
            qreal currentScaleFactor =
                    QLineF(touchPoint0.globalLastPosition(), touchPoint1.globalLastPosition()).length() /
                    QLineF(touchPoint0.globalPressPosition(), touchPoint1.globalPressPosition()).length();
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
SymbolItem::SymbolItem(Logic *lr, QAction *activated, QGraphicsItem *parent) :
    QGraphicsPolygonItem(parent),
    m_lr(lr),
    m_activated(activated)
{
    QPainterPath path;

    // Pre-build basic diagram shapes for each logic symbol
    switch (lr->op)
    {
        case LogicOp::Net:
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
        case LogicOp::ClkGate:
            path.lineTo(10, 0);
            path.lineTo(10, -20);
            path.lineTo(50, 20);
            path.lineTo(50, -20);
            path.lineTo(10, 20);
            path.lineTo(10, 0);
        break;
        case LogicOp::Latch:
            path.lineTo(10, 0);
            path.lineTo(10, 25);
            path.lineTo(50, 25);
            path.lineTo(50,-25);
            path.lineTo(10,-25);
            path.lineTo(10, 0);
        break;
        case LogicOp::DotDot:
            path.lineTo(10, 0);
            break;
    }
    m_poly = path.toFillPolygon();

    setPolygon(m_poly);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    // Items are not movable but this is useful to keep for debug
    setFlag(QGraphicsItem::ItemIsMovable, QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier));
}

/*
 * Do additional painting on each symbol and draw the logic net name
 */
void SymbolItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QString name = m_lr->name;
    auto width = painter->fontMetrics().horizontalAdvance(name);
    constexpr auto maxTextWidth = 40; // Estimated usable width for text inside a symbol

    QRect bounds(0, -25, 48, 50);
    if ((m_lr->op == LogicOp::Or) || (m_lr->op == LogicOp::Nor))
        bounds.setRight(39); // Shift the text a little bit to the left for OR and NOR symbols
    else if (m_lr->op == LogicOp::ClkGate)
    {
        painter->drawText(20, 20, "CLK"); // Clock gate has "CLK" written on the bottom
        if (name.contains(QChar('~'))) // Inverted clock variation has a small circle in the middle
            painter->drawText(25, 10, "O"); // TODO: This feels hacky
        name = QString(); // Do not print net name
    } else if (m_lr->op == LogicOp::DotDot)
        name = ". . .";
    else if (m_lr->op == LogicOp::Latch)
    {
        painter->drawText(15, 20, "Latch");
        painter->drawText(0, -5, "Q");
        width = maxTextWidth; // Force printing the name outside of the latch box
    }

    // Show the number of inputs to this logic gate
    if (m_lr->inputs.count() > 1)
        painter->drawText(57, 20, QString::number(m_lr->inputs.count()));

    // A terminating (leaf) node can accomodate extended text such as the node's tip
    if (m_lr->inputs.count() == 0)
    {
        QString tip = ::controller.getTip().get(m_lr->outnet);

        if (width < maxTextWidth) // The name fits inside the synbol box
        {
            painter->drawText(bounds, Qt::AlignVCenter | Qt::AlignRight, name);
            painter->drawText(55, 5, tip);
        }
        else // The name does not fit, so print it outside, to the right, along with the tip
            painter->drawText(55, 5, name % "  " % tip);
    }
    else if (m_lr->root) // The root node is printed below the symbol
    {
        bounds.setLeft(50 - width - 2); // Expand the bounding box to the left if needed by the text
        painter->drawText(bounds, Qt::AlignBottom | Qt::AlignRight, name);
    }
    else
        painter->drawText(bounds, Qt::AlignVCenter | Qt::AlignRight, name);

    QGraphicsPolygonItem::paint(painter, option, widget);
}

/*
 * Provide the default bounding box for each item: 50x50 for most items,
 * but extend the width for leaf nodes (those with no inputs) and the root node
 */
QRectF SymbolItem::boundingRect() const
{
    QRectF box {0, -25, 50, 50};
    if (m_lr->inputs.count() == 0)
        box.setWidth(150);
    if (m_lr->root)
        box.setLeft(-50);
    return box;
}

void WidgetGraphicsView::contextMenuEvent(QContextMenuEvent *event)
{
    bool haveSelection = scene()->selectedItems().count() > 0;
    m_menu->actions()[0]->setEnabled(haveSelection);
    m_menu->actions()[1]->setEnabled(haveSelection);

    m_menu->exec(mapToGlobal(event->pos()));
}
