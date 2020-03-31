#include "WidgetWaveform.h"
#include <QPainter>
#include <QPaintEvent>

WidgetWaveform::WidgetWaveform(QWidget *parent) : QWidget(parent),
    m_hscale(10),
    m_waveheight(10)
{
    m_lastdataindex = 500;
    for (auto &data : m_data)
    {
        for (uint i=0; i<500; i++)
        {
            data.append(rand());
        }
    }
    m_data[0][0] = 0;
    m_data[0][1] = 0;
    m_data[0][2] = 1;
    m_data[0][3] = 0;

    // Set up few dummy cursors
    m_cursors2x.append(1);
    m_cursors2x.append(10);
    m_cursors2x.append(200);
    // Select the second cursor
    m_cursor = 1;

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setMouseTracking(true);
}

void WidgetWaveform::paintEvent(QPaintEvent *pe)
{
    const QRect &r = pe->rect();
    QPainter painter(this);

    painter.setPen(QPen(QColor(0, 255, 0), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));

    // For each data trace
    uint y = 10;
    for (auto &trace : m_data)
    {
        drawOneSignal(painter, y, trace);
        y += 20;
    }

    drawCursors(painter, r);
}

void WidgetWaveform::drawOneSignal(QPainter &painter, uint y, QVector<pin_t> &data)
{
    pin_t data_prev = data.at(0) & 1;
    for (int i = 0; i < data.length(); i++)
    {
        pin_t data_cur = data.at(i) & 1;

        uint x1 = i * m_hscale;
        uint x2 = (i + 1) * m_hscale;

        uint y2 = data_cur ? m_waveheight : 0;

        if (data_prev != data_cur)
        {
            uint y1 = data_prev ? m_waveheight : 0;
            painter.drawLine(x1, y - y1, x1, y - y2);
        }

        painter.drawLine(x1, y - y2, x2, y - y2);

        data_prev = data_cur;
    }
}

void WidgetWaveform::drawCursors(QPainter &painter, const QRect &r)
{
    painter.setPen(QPen(QColor(255, 255, 0), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    for (auto cursorX : m_cursors2x)
    {
        uint x = cursorX * m_hscale / 2.0;
        painter.drawLine(x, r.top(), x, r.bottom());
    }
    // Draw the selected cursor on top
    if (m_cursor < uint(m_cursors2x.count()))
    {
        painter.setPen(QPen(QColor(255, 239, 0), 3, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        uint x = m_cursors2x.at(m_cursor) * m_hscale / 2.0;
        painter.drawLine(x, r.top(), x, r.bottom());
    }
}

QSize WidgetWaveform::sizeHint() const
{
    // The total data size plus some (~12%), to give space on the right end
    uint extra = m_lastdataindex >> 3;
    QSize size((m_lastdataindex + extra) * m_hscale, 0);
    return size;
}

//============================================================================
// Mouse tracking
//============================================================================

void WidgetWaveform::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();

    if(m_mousePressed && m_cursormoving)
    {
        uint mouse_in_dataX = m_mousePos.x() / (m_hscale / 2);
        if ((mouse_in_dataX >= 0) && (mouse_in_dataX <= m_lastdataindex * 2))
            m_cursors2x[m_cursor] = mouse_in_dataX;
        update();
    }
}

void WidgetWaveform::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        return; // handled via customContextMenuRequested() signal
    m_pinMousePos = event->pos();
    m_mousePressed = true;

    if (m_cursors2x.count()) // The following code moves cursors
    {
        // On a mouse click, try to find the cursor that is close to the mouse pointer (off by a few pixels)
        // If found, make that cursor active and ready to move it. If not, move the currently active cursor
        // to the mouse position and have it ready to move.
        int mouseX = m_mousePos.x();
        for (int i=0; i<m_cursors2x.count(); i++)
        {
            int data_to_screenX = m_cursors2x.at(i) * (m_hscale / 2);
            if (abs(mouseX - data_to_screenX) < 5)
                m_cursor = i;
        }
        uint mouse_in_dataX = m_mousePos.x() / (m_hscale / 2);
        if ((mouse_in_dataX >= 0) && (mouse_in_dataX <= m_lastdataindex * 2))
            m_cursors2x[m_cursor] = mouse_in_dataX;
        m_cursormoving = true;
        update();
    }
}

void WidgetWaveform::mouseReleaseEvent (QMouseEvent *)
{
    m_mousePressed = false;
    m_cursormoving = false;
}

void WidgetWaveform::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0)
        m_hscale *= 1.2;
    else
        m_hscale /= 1.2;
    updateGeometry();
}

void WidgetWaveform::leaveEvent(QEvent *)
{
    m_mousePressed = false;
    m_cursormoving = false;
}

void WidgetWaveform::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}
