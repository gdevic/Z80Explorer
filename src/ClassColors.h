#ifndef CLASSCOLORS_H
#define CLASSCOLORS_H

#include <QObject>
#include <QColor>
#include <QHash>

// Invalid color (pick an ugly one or an odd number easy to spot)
#define COLOR_INVALID  (QRgb(0xCDCDCD))

/*
 * This class contains and manages colors used by the application
 */
class ClassColors : public QObject
{
    Q_OBJECT
public:
    explicit ClassColors(QObject *parent = nullptr);

    QColor &get(uint net)               // Returns the color of a net as QColor
        { return m_colors.contains(net) ? m_colors[net] : m_invalid; }

    uint16_t get16(uint net)            // Returns the color of a net as 565 rgb
        { return toUint16(get(net)); }

    uint16_t toUint16(QColor &c)        // Converts from color to uint16_t 565 rgb
        { return ((uint16_t(c.red()) & 0xF8) << 8)
               | ((uint16_t(c.green()) & 0xFC) << 3)
               | ((uint16_t(c.blue())) >> 3); }

private:
    QHash<uint, QColor> m_colors;       // Hash of net numbers to their custom colors
    QColor m_invalid { COLOR_INVALID }; // Color value of the invalid color
};

#endif // CLASSCOLORS_H
