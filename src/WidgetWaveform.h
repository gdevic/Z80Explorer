#ifndef WIDGETWAVEFORM_H
#define WIDGETWAVEFORM_H

#include <QWidget>

typedef uint8_t  pin_t;                 // Type of the pin state (0, 1; or 2 for floating)

class WidgetWaveform : public QWidget
{
    Q_OBJECT
public:
    explicit WidgetWaveform(QWidget *parent = nullptr);

    void paintEvent(QPaintEvent *);

    // XXX Sample data
#define MAX_DATA  10
    QVector<pin_t> m_data[MAX_DATA];
    uint m_lastdataindex;               // Index of the last data point (aggregate, any)

    void drawOneSignal(QPainter &painter, uint y, QVector<pin_t> &data);
    qreal m_hscale;                     // Horizontal scale factor
    uint m_waveheight;                  // Wave height in pixels

    void drawCursors(QPainter &painter, const QRect &r);
    QVector<uint> m_cursors2x {};       // Cursors' locations, index into the data (times 2)
    uint m_cursor {};                   // Index of the active cursor
    bool m_cursormoving {};             // Selected cursor is being moved by the mouse

    QSize sizeHint() const override;


    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void wheelEvent(QWheelEvent *);
    void leaveEvent(QEvent* event);
    void keyPressEvent(QKeyEvent *e);

    QPoint  m_mousePos;                 // Current mouse position
    QPoint  m_pinMousePos;              // Mouse position at the time of button press
    bool    m_mousePressed;             // Mouse button is pressed

};

#endif // WIDGETWAVEFORM_H
