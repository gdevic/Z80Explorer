#ifndef CLASSCOLORS_H
#define CLASSCOLORS_H

#include "AppTypes.h"
#include <QObject>
#include <QColor>
#include <QHash>

// Contains a coloring definition
struct colordef
{
    QString expr;                       // Expression condition
    QColor color {QColor(Qt::black)};   // Color
    uint method {1};                    // Name matching method
};

/*
 * This class contains and manages custom nets coloring used by the application
 */
class ClassColors : public QObject
{
    Q_OBJECT
public:
    explicit ClassColors(QObject *parent = nullptr);

    bool isDefined(net_t net)           // Returns true if a net has a defined custom color
        { return m_colors.contains(net); }

    const QColor &get(net_t net)        // Returns the color of a net as QColor
        { return m_colors.contains(net) ? m_colors[net] : m_colors[0]; }

    uint16_t get16(net_t net)           // Returns the color of a net as 565 rgb
        { return toUint16(get(net)); }

    uint16_t toUint16(const QColor &c)  // Converts from color to uint16_t 565 rgb
        { return ((uint16_t(c.red()) & 0xF8) << 8)
               | ((uint16_t(c.green()) & 0xFC) << 3)
               | ((uint16_t(c.blue())) >> 3); }

    void update();                      // Updates internal color table
    bool load(QString dir);             // Loads color definitions
    bool save(QString dir);             // Saves color definitions

public slots:
    void onShutdown();                  // Called when the app is closing

private:
    QHash<net_t, QColor> m_colors;      // Hash of net numbers to their custom colors
    QVector<colordef> m_colordefs;      // Coloring definitions
};

#endif // CLASSCOLORS_H
