#include "WidgetPane.h"
#include <QPainter>
#include <QPaintEvent>

WidgetPane::WidgetPane(QWidget *parent) : QWidget(parent)
{
}

void WidgetPane::paintEvent(QPaintEvent *pe)
{
    const QRect &r = pe->rect();
    QPainter painter(this);

    for (uint i=0; i < 10; i++)
    {
        painter.setBrush(QColor(rand()%255,rand()%255,rand()%255));
        painter.setPen(QPen(QColor(255,rand(),rand()), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        painter.drawRect(rand()%r.width()+10,rand()%r.height()+10,rand()%r.width()-20,rand()%r.height()-20);
    }
}
