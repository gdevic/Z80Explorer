#ifndef CLASSCOLORS_H
#define CLASSCOLORS_H

#include "AppTypes.h"
#include <QObject>
#include <QColor>
#include <QHash>

/*
 * This class contains and manages colors used by the application
 */
class ClassColors : public QObject
{
    Q_OBJECT
public:
    explicit ClassColors(QObject *parent = nullptr);

    bool isDefined(net_t net)           // Returns true if a net has a defined custom color
        { return m_colors.contains(net); }

    QColor &get(net_t net)              // Returns the color of a net as QColor
        { return m_colors.contains(net) ? m_colors[net] : m_colors[0]; }

    uint16_t get16(net_t net)           // Returns the color of a net as 565 rgb
        { return toUint16(get(net)); }

    uint16_t toUint16(QColor &c)        // Converts from color to uint16_t 565 rgb
        { return ((uint16_t(c.red()) & 0xF8) << 8)
               | ((uint16_t(c.green()) & 0xFC) << 3)
               | ((uint16_t(c.blue())) >> 3); }

private:
    QHash<net_t, QColor> m_colors;      // Hash of net numbers to their custom colors
};

#endif // CLASSCOLORS_H
