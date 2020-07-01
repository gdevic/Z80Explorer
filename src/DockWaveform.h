#ifndef DOCKWAVE_H
#define DOCKWAVE_H

#include "AppTypes.h"
#include <QDockWidget>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QScrollArea>

namespace Ui { class DockWaveform; }

// Defines one view item
struct viewitem
{
    QString name;               // Net or bus name
    net_t net { 0 };            // Net number or 0 if bus
    uint format { 0 };          // Format to use to display values
    QColor color { Qt::green }; // Color override to use to display waveform

    template <class Archive> void serialize(Archive & ar) { ar(name, format, color); }

    viewitem(const QString name): name(name) {}
    viewitem(const QString name, const net_t n): name(name), net(n) {}
    viewitem(){};
    bool operator==(const viewitem &b) { return name == b.name; }
};
Q_DECLARE_METATYPE(viewitem);

/*
 * Subclass QScrollArea to handle PgUp/PgDown (and cursor Up/Down) keys for zooming in/out
 */
class CustomScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit CustomScrollArea(QWidget *parent = nullptr): QScrollArea(parent) {}

signals:
    void zoom(bool isUp);       // Send a zoom signal with the direction indicator
    void enlarge(int delta);    // Send a vertical enlarge signal with the increment value (-1,+1)

private:
    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key())   // Intercept and handle only PgUp/PgDown and cursor Up/Down keys
        {
        case Qt::Key_Up: emit enlarge(1); break;
        case Qt::Key_PageUp: emit zoom(true); break;
        case Qt::Key_Down: emit enlarge(-1); break;
        case Qt::Key_PageDown: emit zoom(false); break;
        default: QScrollArea::keyPressEvent(event);
        }
    }

    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel) // If the user pressed Ctrl key, we handle the wheel event
            return QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        return false;
    }
};

/*
 * This class implements waveform window and container view
 */
class DockWaveform : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockWaveform(QWidget *parent, uint id);
    ~DockWaveform();

    inline viewitem *getFirst(int &it)  // Iterator
        { it = 1; return (m_view.count() > 0) ? m_view.data() : nullptr; }
    inline viewitem *getNext(int &it)   // Iterator
        { return (it < m_view.count()) ? &m_view[it++] : nullptr; }

private slots:
    void onLoad();                      // Load view items from a file
    void onSaveAs();                    // Save current set of view items with a new file name
    void onSave();                      // Save current set of view items
    void onPng();                       // Exports window view as a PNG image file
    void onEdit();                      // Edit current set of view items
    void scroll(int deltaX);            // User moved the view, scroll it
    void cursorChanged(uint hcycle);    // Cursor moved, need to update values that are shown
    void onScrollBarActionTriggered(int);
    void onScrollBarRangeChanged(int,int);
    void onEnlarge(int delta);          // User resized the view vertically
    void eventNetName(Netop op, const QString name, const net_t net);

private:
    void rebuildList();
    bool load(QString fileName);        // Loads waveform items
    bool save(QString fileName);        // Saves waveform items

    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel) // If the user pressed Ctrl key, we handle the wheel event
            return QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        return false;
    }

private:
    Ui::DockWaveform *ui;
    uint m_id;                          // This dock ID value

    QVector<viewitem> m_view;           // A collection of view items

    uint m_lastcursor;                  // Last cursor cycle value
    QString m_fileViewlist;             // This window's default waveform view configuration file name
    qreal m_rel {};                     // Relative waveform scroll slider position
    int m_sectionSize;                  // Table vertical section size in pixels

    QStringList getNames();             // Returns a list of all view item names
    viewitem *find(QString name);       // Find a view item with the given name or nullptr
    viewitem *find(net_t n);            // Find a view item with the given net number or nullptr
    void add(QString name);             // Add a view item
    void updateViewitems(QStringList);  // Update view items based on the new list of nets/buses
};

#endif // DOCKWAVE_H
