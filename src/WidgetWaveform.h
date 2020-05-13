#ifndef WIDGETWAVEFORM_H
#define WIDGETWAVEFORM_H

#include "ClassWatch.h"
#include "DockWaveform.h"
#include <QWidget>
#include <QTimer>

class WidgetWaveform : public QWidget
{
    Q_OBJECT
public:
    explicit WidgetWaveform(QWidget *parent = nullptr);
    void setDock(DockWaveform *dock)    // Sets a pointer to our UI parent dock
        { m_dock = dock; }

signals:
    void cursorChanged(uint hcycle);    // Cursor selected a new hcycle
    void scroll(int deltaX);            // User moved the view, request to scroll it
    void setLink(int value);            // Sets the link button text to a numeric value

public slots:
    void onLinked(bool isLinked);       // Links and unlinks the first two cursors
    void onZoom(bool isUp);             // Zooms in and out by a predefined step

private slots:
    void onRunStopped(uint);            // Called by the sim when the current run stops at a given half-cycle
    void onTimeout() { update(); }      // Refresh graph when running simulation

private:
    DockWaveform *m_dock {};
    void drawOneSignal_Net(QPainter &painter, uint y, uint hstart, watch *watch, viewitem *viewitem);
    void drawOneSignal_Bus(QPainter &painter, uint y, uint hstart, watch *watch, viewitem *viewitem);
    qreal m_hscale {10};                // Horizontal scale factor
    const uint m_waveheight {15};       // Wave height in pixels

    void drawCursors(QPainter &painter, const QRect &r, uint hstart);
    QVector<uint> m_cursors2x {};       // Cursors' locations, index into the data (times 2)
    int m_linked {};                    // First two cursors are linked together by this delta; 0 for unlinked
    uint m_cursor {};                   // Index of the active cursor
    bool m_cursormoving {};             // Selected cursor is being moved by the mouse

    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void leaveEvent(QEvent *) override;
    QSize sizeHint() const override;

    QPoint  m_mousePos;                 // Current mouse position
    QPoint  m_pinMousePos;              // Mouse position at the time of button press
    bool    m_mousePressed;             // Mouse button is pressed
    int     m_fontheight;               // Cursor flag font height in pixels

    QTimer m_timer;                     // Refresh graph every twice a second
};

#endif // WIDGETWAVEFORM_H
