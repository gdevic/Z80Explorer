#include "ClassColors.h"

ClassColors::ClassColors(QObject *parent) : QObject(parent)
{
    // Populate some known colors
    m_colors[0] = QColor(Qt::black);        // Also used as an invalid color
    m_colors[1] = QColor(  0, 127,   0);    // vss medium green
    m_colors[2] = QColor(192,   0,   0);    // vcc quite red
    m_colors[3] = QColor(192,192,192);      // clk subdued white
}
