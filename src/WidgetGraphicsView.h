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
 * This class, based on QGraphicsPolygonItem, contains code to instantiate our logic symbols
 */
class SymbolItem : public QGraphicsPolygonItem
{
public:
    enum { Type = UserType + 15 };

    SymbolItem(Logic *lr, QGraphicsItem *parent = nullptr);
    int type() const override { return Type; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QPolygonF m_poly;                   // Prebuilt basic symbol shape
    Logic *m_lr;                        // Link to the logic structure it represents
};

#endif // WIDGETGRAPHICSVIEW_H
