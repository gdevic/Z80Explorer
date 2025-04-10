#ifndef WIDGETGRAPHICSVIEW_H
#define WIDGETGRAPHICSVIEW_H

#include "ClassLogic.h"
#include <QGraphicsPolygonItem>
#include <QGraphicsView>

/*
 * This class, based on QGraphicsView, contains code to implement logic diagram view
 * It extends the base class with mouse (wheel zoom) handling and pinch-to-zoom on
 * touch displays
 */
class WidgetGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit WidgetGraphicsView(QWidget *parent);
    QMenu *m_menu;                      // Context menu used by this view (set in the DialogSchematic constructor)

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void wheelEvent(QWheelEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    qreal m_scale {1.0};                // View zoom / scale factor
};

/*
 * This class, based on QGraphicsPolygonItem, contains code to instantiate each of our logic symbol types
 */
class SymbolItem : public QGraphicsPolygonItem
{
public:
    SymbolItem(Logic *lr, QAction *activated, QGraphicsItem *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QRectF boundingRect() const override;
    const QString get()                 // Returns the output net name of this logic gate
        { return m_lr->name; }
    net_t getNet()                      // Returns the output net number of this logic gate
        { return m_lr->outnet; }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override
        { m_activated->trigger(); }     // On a double-click, trigger the assigned action

private:
    QPolygonF m_poly;                   // Prebuilt basic symbol shape
    Logic *m_lr;                        // Link to the logic structure it represents
    QAction *m_activated;               // Action to take when the symbol is double-clicked
};

#endif // WIDGETGRAPHICSVIEW_H
