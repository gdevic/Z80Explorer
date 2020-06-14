#ifndef WIDGETGRAPHICSVIEW_H
#define WIDGETGRAPHICSVIEW_H

#include "ClassLogic.h"
#include <QGraphicsPolygonItem>
#include <QGraphicsView>

/*
 * This class, based on QGraphicsView, contains code to implement logic diagram view
 * It extends the base class with mouse (wheel zoom) handling
 */
class WidgetGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit WidgetGraphicsView(QWidget *parent = nullptr);

private:
    void wheelEvent(QWheelEvent *event) override;

private:
    qreal m_scale {1.0};                // View zoom / scale factor
};

/*
 * This class, based on QGraphicsPolygonItem, contains code to instantiate each of our logic symbol types
 */
class SymbolItem : public QGraphicsPolygonItem
{
public:
    SymbolItem(Logic *lr, QMenu *menu, QGraphicsItem *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    const QString get()                 // Returns the net name that this logic symbol represents
        { return m_lr->name; }
    net_t getNet()                      // Returns the net number that this logic symbol represents
        { return m_lr->net; }

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;

private:
    QPolygonF m_poly;                   // Prebuilt basic symbol shape
    Logic *m_lr;                        // Link to the logic structure it represents
    QMenu *m_menu;                      // Context menu used by this schematic object
};

#endif // WIDGETGRAPHICSVIEW_H
