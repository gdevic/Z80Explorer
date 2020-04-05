#ifndef WIDGETWAVEFORM_H
#define WIDGETWAVEFORM_H

#include <ClassWatch.h>
#include <QWidget>
#include <QTimer>

class WidgetWaveform : public QWidget
{
    Q_OBJECT
public:
    explicit WidgetWaveform(QWidget *parent = nullptr);

private slots:
    void onRunStopped();                // Signal from the controller that the simulation run stopped
    void onTimeout() { update(); }      // Refresh graph when running simulation

private:
    void drawOneSignal(QPainter &painter, uint y, uint hstart, watch *watch);
    qreal m_hscale;                     // Horizontal scale factor
    uint m_waveheight;                  // Wave height in pixels

    void drawCursors(QPainter &painter, const QRect &r, uint hstart);
    QVector<uint> m_cursors2x {};       // Cursors' locations, index into the data (times 2)
    uint m_cursor {};                   // Index of the active cursor
    bool m_cursormoving {};             // Selected cursor is being moved by the mouse

    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent *e) override;
    QSize sizeHint() const override;

    QPoint  m_mousePos;                 // Current mouse position
    QPoint  m_pinMousePos;              // Mouse position at the time of button press
    bool    m_mousePressed;             // Mouse button is pressed

    QTimer m_timer;                     // Refresh graph every twice a second
};

#endif // WIDGETWAVEFORM_H
