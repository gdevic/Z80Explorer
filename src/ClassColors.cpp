#include "ClassColors.h"

ClassColors::ClassColors(QObject *parent) : QObject(parent)
{
    // Populate some known colors
    m_colors[1] = QColor(  0, 127,   0); // vss dark green
    m_colors[2] = QColor(255,   0,   0); // vcc red
    m_colors[3] = QColor(200, 200, 200); // clk white-ish
}
