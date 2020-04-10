#include "WidgetWaveform.h"
#include "ClassController.h"
#include "DockWaveform.h"
#include <QtGlobal>
#include <QPainter>
#include <QPaintEvent>
#include <QStringBuilder>

WidgetWaveform::WidgetWaveform(QWidget *parent) : QWidget(parent),
    m_hscale(10),
    m_waveheight(15)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &WidgetWaveform::onTimeout);

    connect(&::controller, SIGNAL(onRunStopped()), this, SLOT(onRunStopped()));

    // Set up two initial cursors
    m_cursors2x.append(1);
    m_cursors2x.append(10);

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

    // Set up font to draw bus values
    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);

    // Draw a faint gray background grid
    painter.setPen(QPen(Qt::gray));
    const uint min_dist = 50;
    uint delta = 1 + qreal(min_dist) / m_hscale;
    for (int i = 0; i <= MAX_WATCH_HISTORY; i += delta)
    {
        uint x = i * m_hscale;
        painter.drawLine(x, 0, x, r.bottom());
    }

    uint hstart = ::controller.getWatch().gethstart();
    uint y = 16; // Starting Y coordinate, the bottom of the first net
    int it;
    viewitem *vi = m_dock->getFirst(it);
    while (vi != nullptr)
    {
        watch *w = ::controller.getWatch().find(vi->name);
        if (w) // Check that the view item is actually being watched
        {
            if (w->n)
                drawOneSignal_Net(painter, y, hstart, w, vi);
            else
                drawOneSignal_Bus(painter, y, hstart, w, vi);
        }
        y += 20; // Advance to the next Y coordinate, this is the height of each net
        vi = m_dock->getNext(it);
    }
    drawCursors(painter, r, hstart);
}

void WidgetWaveform::drawOneSignal_Net(QPainter &painter, uint y, uint hstart, watch *w, viewitem *viewitem)
{
    painter.setPen(viewitem->color);
    net_t data_cur, data_prev = ::controller.getWatch().at(w, hstart);
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
            painter.drawLine(x1, y - y2, x2, y - y2);
        }
        painter.drawLine(x1, y - y2, x2, y - y2);
    }
}

void WidgetWaveform::drawOneSignal_Bus(QPainter &painter, uint y, uint hstart, watch *w, viewitem *viewitem)
{
    painter.setPen(viewitem->color);
    uint width;
    uint data_cur, data_prev = ::controller.getWatch().at(w, hstart, width);
    uint last_data_x = 0; // X coordinate of the last bus data change
    QString text; // Text value of the last bus data
    for (int i = 0; i < MAX_WATCH_HISTORY; i++, data_prev = data_cur)
    {
        data_cur = ::controller.getWatch().at(w, hstart + i, width);
        if (width == 0) // If the data is undef or incomplete at this cycle, skip drawing it
            continue;
        uint x1 = i * m_hscale;
        uint x2 = (i + 1) * m_hscale;
        uint y1 = y;
        uint y2 = y - m_waveheight;
        if (data_prev != data_cur) // Bus data is changing, draw crossed lines
        {
            // Check if there is enough space (in pixels) to write out last bus data value
            QRect bb = painter.fontMetrics().boundingRect(text);
            if (bb.width() < int(x1 - last_data_x))
            {
                painter.setPen(QPen(Qt::white));
                painter.drawText(last_data_x + 3, y1 - 2, text);
                painter.setPen(viewitem->color);
            }

            painter.drawLine(x1, y1, x1+3, y2);
            painter.drawLine(x1, y2, x1+3, y1);
            painter.drawLine(x1+3, y1, x2, y1);
            painter.drawLine(x1+3, y2, x2, y2);

            last_data_x = x1;
            // Format the text of the new bus data value
            text = QString::number(width) % "'h" % QString::number(data_cur, 16);
        }
        else // Bus data is the same, continue drawing two parallel horizontal lines
        {
            painter.drawLine(x1, y1, x2, y1);
            painter.drawLine(x1, y2, x2, y2);
        }
    }
}

void WidgetWaveform::drawCursors(QPainter &painter, const QRect &r, uint hstart)
{
    // Set up font to draw cursor values
    QFont font = painter.font();
    font.setPixelSize(14);
    painter.setFont(font);

    QFontMetrics fontm = QFontMetrics(painter.font());
    m_fontheight = fontm.height();
    QPen pen(Qt::yellow);
    painter.setPen(pen);

    for (int i=0; i<m_cursors2x.count(); i++)
    {
        uint cursorX = m_cursors2x.at(i);
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
        uint x = m_cursors2x[i] * m_hscale / 2.0;
        painter.drawLine(x, r.top(), x, r.bottom() - i * m_fontheight + 1);

        emit cursorChanged(m_cursors2x[m_cursor] / 2 + hstart);
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
    m_hscale = qBound(1.0, m_hscale, 100.0);
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
