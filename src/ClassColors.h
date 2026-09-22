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

    const QColor &get(net_t net)        // Returns a custom color of a net
        { return m_colors.contains(net) ? m_colors[net] : m_colors[0]; }
    const QColor getActive()            // Returns the default color of an active net
        { return QColor(255,0,255); }
    const QColor getInactive()          // Returns the default color of an inactive net
        { return QColor(128,0,128); }
    const QColor getVss()               // Returns the default vss net color: medium green
        { return QColor(0,127,0); }
    const QColor getVcc()               // Returns the default vcc net color: red-ish
        { return QColor(172,0,0); }

    void rebuild();                     // Updates internal color table
    bool load(QString fileName, bool merge = false); // Loads or merges color definitions
    bool save(QString fileName);        // Saves color definitions
    QString getFileName()               // Returns the current colors file name
        { return m_jsonFile; }
    bool inhibitAutoSave() const        // True after a merge until the user explicitly saves
        { return m_inhibitAutoSave; }

    const QStringList getMatchingMethods()
        { return {"Exact match", "Starts with", "Regex", "Net number"}; };
    const QVector<colordef> &getColordefs()
        { return m_colordefs; }         // Used by the colors editor dialog
    void setColordefs(QVector<colordef>); // Sets a new colordefs array

private:
    QHash<net_t, QColor> m_colors;      // Hash of net numbers to their custom colors
    QVector<colordef> m_colordefs;      // Coloring definitions
    QString m_jsonFile;                 // File name used to load colors
    bool m_inhibitAutoSave {false};     // Set by a merge; cleared by an explicit save; suppresses the registry save
};

#endif // CLASSCOLORS_H
