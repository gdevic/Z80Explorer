#ifndef CLASSCOLORS_H
#define CLASSCOLORS_H

#include "AppTypes.h"
#include <QColor>
#include <QHash>
#include <QObject>

// Contains a coloring definition
struct colordef
{
    QString expr;                       // Expression condition
    uint method {1};                    // Name matching method
    QColor color {QColor(Qt::yellow)};  // Color
    bool enabled {true};                // Enables this coloring pattern
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

    void rebuild();                     // Updates internal color table
    bool load(QString fileName, bool merge = false); // Loads or merges color definitions
    bool save(QString fileName);        // Saves color definitions
    QString getFileName()               // Returns the current colors file name
        { return m_jsonFile; }

    const QStringList getMatchingMethods()
        { return {"Exact match", "Starts with", "Regex", "Net number"}; };
    const QVector<colordef> &getColordefs()
        { return m_colordefs; }         // Used by the colors editor dialog
    void setColordefs(QVector<colordef>); // Sets a new colordefs array

public slots:
    void onShutdown();                  // Called when the app is closing

private:
    QHash<net_t, QColor> m_colors;      // Hash of net numbers to their custom colors
    QVector<colordef> m_colordefs;      // Coloring definitions
    QString m_jsonFile;                 // File name used to load colors
};

#endif // CLASSCOLORS_H
