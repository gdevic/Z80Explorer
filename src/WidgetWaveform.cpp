#include "WidgetWaveform.h"
#include "ClassController.h"
#include <QPainter>
#include <QPaintEvent>

WidgetWaveform::WidgetWaveform(QWidget *parent) : QWidget(parent),
    m_hscale(10),
    m_waveheight(10)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &WidgetWaveform::onTimeout);

    connect(&::controller, SIGNAL(onRunStopped()), this, SLOT(onRunStopped()));

    // Set up few dummy cursors
    m_cursors2x.append(1);
    m_cursors2x.append(10);
    // Select the second cursor
    m_cursor = 1;

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setMouseTracking(true);
    m_timer.start();
}

/*
 * Controller signals us that the current simulation run completed
 */
void WidgetWaveform::onRunStopped()
{
    update(); // Repaint the view
}

void WidgetWaveform::paintEvent(QPaintEvent *pe)
{
    const QRect &r = pe->rect();
    QPainter painter(this);
    painter.setPen(QPen(QColor(0, 255, 0), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    uint hstart = ::controller.getWatch().gethstart();
    uint y = 10;
    int it;
    watch *w = ::controller.getWatch().getFirst(it);
    while (w != nullptr)
    {
        if (w->enabled)
            drawOneSignal(painter, y, hstart, w);
        y += 20;
        w = ::controller.getWatch().getNext(it);
    }
    drawCursors(painter, r, hstart);
}

void WidgetWaveform::drawOneSignal(QPainter &painter, uint y, uint hstart, watch *w)
{
    net_t data_prev = ::controller.getWatch().at(w, hstart);
    pin_t data_cur;
    for (int i = 0; i < MAX_WATCH_HISTORY; i++, data_prev = data_cur)
    {
        data_cur = ::controller.getWatch().at(w, hstart + i);
        if (data_cur > 2) // If the data is undef at this cycle, skip drawing it
            continue;
        uint x1 = i * m_hscale;
        uint x2 = (i + 1) * m_hscale;
        uint y2 = data_cur ? m_waveheight : 0;
        if (data_prev != data_cur)
        {
            uint y1 = data_prev ? m_waveheight : 0;
            painter.drawLine(x1, y - y1, x1, y - y2);
        }
        painter.drawLine(x1, y - y2, x2, y - y2);
    }
}

void WidgetWaveform::drawCursors(QPainter &painter, const QRect &r, uint hstart)
{
    QFont font = painter.font();
    font.setPixelSize(14);
    painter.setFont(font);

    QFontMetrics fontm = QFontMetrics(painter.font());
    m_fontheight = fontm.height();
    QPen pen(Qt::yellow);
    painter.setPen(pen);

    for (int i=0; i<m_cursors2x.count(); i++)
    {
        uint cursorX = m_cursors2x[i];
        uint x = cursorX * m_hscale / 2.0;
        uint y = r.bottom() - i * m_fontheight;
        painter.drawLine(x, r.top(), x, y);

        QString text = QString::number((cursorX + hstart) / 2);
        QRect bb = painter.fontMetrics().boundingRect(text);
        bb.adjust(x, y, x, y);

        painter.drawText(x + 2, r.bottom() - i * m_fontheight, text);
        painter.drawRect(bb.adjusted(0, 0, 4, 0));
    }

    // Draw the selected cursor on top
    if (m_cursor < uint(m_cursors2x.count()))
    {
        pen.setWidth(3);
        painter.setPen(pen);
        uint i = m_cursor;
        uint x = m_cursors2x.at(m_cursor) * m_hscale / 2.0;
        painter.drawLine(x, r.top(), x, r.bottom() - i * m_fontheight + 1);
    }
}

QSize WidgetWaveform::sizeHint() const
{
    // The total data size plus some (~12%), to give space on the right end
    uint extra = MAX_WATCH_HISTORY >> 3;
    QSize size((MAX_WATCH_HISTORY + extra) * m_hscale, 0);
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
        if ((mouse_in_dataX >= 0) && (mouse_in_dataX <= MAX_WATCH_HISTORY * 2))
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
        // On a mouse click, identify the cursor to be used:
        // If the mouse is on the bottom row (where the cursors' flags are), use the corresponding cursor
        QRect r = geometry();
        if (m_mousePos.y() > (r.bottom() - m_cursors2x.count() * m_fontheight))
            m_cursor = (r.bottom() - m_mousePos.y()) / m_fontheight;
        else
        // Next, try to find the cursor that is close to the mouse pointer (off by a few pixels)
        // If found, make that cursor active and ready to move it. If not, move the currently active cursor
        // to the mouse position and have it ready to move.
        for (int i=0; i<m_cursors2x.count(); i++)
        {
            int data_to_screenX = m_cursors2x.at(i) * (m_hscale / 2);
            if (abs(m_mousePos.x() - data_to_screenX) < 10)
                m_cursor = i;
        }
        uint mouse_in_dataX = m_mousePos.x() / (m_hscale / 2);
        if ((mouse_in_dataX >= 0) && (mouse_in_dataX <= MAX_WATCH_HISTORY * 2))
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
