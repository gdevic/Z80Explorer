#include "WidgetWaveform.h"
#include "ClassController.h"
#include "DockWaveform.h"
#include <QtGlobal>
#include <QPainter>
#include <QPaintEvent>
#include <QStringBuilder>

WidgetWaveform::WidgetWaveform(QWidget *parent) : QWidget(parent)
{
    // Refresh graph when running simulation and when stopped
    connect(&::controller, &ClassController::onRunHeartbeat, this, [this](){ update(); });
    connect(&::controller, &ClassController::onRunStopped, this, [this](){ update(); });

    // Set up two initial cursors
    m_cursors2x.append(1);
    m_cursors2x.append(10);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setMouseTracking(true);
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
    static const QPen penHiZ = QPen(QColor(Qt::white), 1, Qt::DotLine);
    painter.setPen(viewitem->color);
    net_t data_cur, data_prev = ::controller.getWatch().at(w, hstart);
    for (int i = 0; i < MAX_WATCH_HISTORY; i++, data_prev = data_cur)
    {
        data_cur = ::controller.getWatch().at(w, hstart + i);
        if (data_cur > 2) // If the data is undef at this cycle, do not draw it
            continue;
        static const uint wh[4] { 0, m_waveheight, m_waveheight / 2, 0 };
        uint x1 = i * m_hscale;
        uint x2 = (i + 1) * m_hscale;
        uint y1 = wh[data_prev & 3];
        uint y2 = wh[data_cur & 3];

        if (viewitem->format == ClassController::FormatNet::Logic) // Draw simple logic diagram
        {            
            if (Q_UNLIKELY(data_cur == 2)) // Hi-Z states use special color + dotted lines
                painter.setPen(penHiZ);
            if (data_prev != data_cur)
            {
                painter.drawLine(x1, y - y1, x1, y - y2);
                painter.drawLine(x1, y - y2, x2, y - y2);
            }
            painter.drawLine(x1, y - y2, x2, y - y2);
            if (Q_UNLIKELY(data_cur == 2))
                painter.setPen(viewitem->color);
        }
        else if (data_prev != data_cur) // Draw transition triangles
        {
            bool is_up = data_prev < data_cur;
            uint d = 1 + m_hscale / 5; // Stretch triangles as the scale moves up
            QBrush brush = painter.brush();
            painter.setBrush(QBrush(viewitem->color));
            if (!is_up && (viewitem->format != ClassController::FormatNet::TransUp)) // TransDown or TransAny
            {
                QPoint shape[3] = { QPoint(x1-d,y-y1), QPoint(x1,y), QPoint(x1+d,y-y1) };
                painter.drawPolygon(shape, 3);
            }
            if (is_up && (viewitem->format != ClassController::FormatNet::TransDown)) // TransUp or TransAny
            {
                QPoint shape[3] = { QPoint(x1-d,y), QPoint(x1,y-y2), QPoint(x1+d,y) };
                painter.drawPolygon(shape, 3);
            }
            painter.setBrush(brush);
        }
    }
}

void WidgetWaveform::drawOneSignal_Bus(QPainter &painter, uint y, uint hstart, watch *w, viewitem *viewitem)
{
    static const QPen penHiZ = QPen(QColor(Qt::white), 1, Qt::DotLine);
    static const QPen penText = QPen(Qt::white);
    painter.setPen(viewitem->color);
    uint width, data_cur, data_prev = ::controller.getWatch().at(w, hstart, width);
    uint last_data_x = 0; // X coordinate of the last bus data change
    bool width0text = true; // Not too elegant way to ensure we print only once on a stream of undef values
    // Get the text of the initial bus data value
    QString text = ::controller.formatBus(viewitem->format, data_prev, width);
    for (int i = 0; i < MAX_WATCH_HISTORY; i++, data_prev = data_cur)
    {
        uint x1 = i * m_hscale;
        uint x2 = (i + 1) * m_hscale;
        uint y1 = y;
        uint y2 = y - m_waveheight;

        data_cur = ::controller.getWatch().at(w, hstart + i, width);
        if (width == 0) // If the data is undef or incomplete at this cycle, do not draw it
        {
            if (width0text) // Print the text only on the first undef data to flush out previous value
            {
                painter.setPen(penText);
                painter.drawText(last_data_x + 3, y1 - 2, text);
                painter.setPen(viewitem->color);
                last_data_x = x1;
                width0text = false; // Do not print text next time in the undef case
            }
            continue;
        }
        width0text = true; // If we are here, we had a valid data, so enable printing text on undef next time

        if (Q_UNLIKELY(data_cur == UINT_MAX)) // Hi-Z states use special color + dotted lines
            painter.setPen(penHiZ);
        if (data_prev != data_cur) // Bus data is changing, draw crossed lines
        {
            // Check if there is enough space (in pixels) to write out the last text
            QRect bb = painter.fontMetrics().boundingRect(text);
            if (bb.width() < int(x1 - last_data_x))
            {
                painter.save();
                painter.setPen(penText);
                painter.drawText(last_data_x + 3, y1 - 2, text);
                painter.restore();
            }
            last_data_x = x1;

            painter.drawLine(x1, y1, x1+3, y2);
            painter.drawLine(x1, y2, x1+3, y1);
            painter.drawLine(x1+3, y1, x2, y1);
            painter.drawLine(x1+3, y2, x2, y2);

            // Format the text of the new bus data value
            text = ::controller.formatBus(viewitem->format, data_cur, width);
        }
        else // Bus data is the same, continue drawing two parallel horizontal lines
        {
            painter.drawLine(x1, y1, x2, y1);
            painter.drawLine(x1, y2, x2, y2);
        }
        if (Q_UNLIKELY(data_cur == UINT_MAX))
            painter.setPen(viewitem->color);

        // At the end of the graph, write out last bus values
        if (i == (MAX_WATCH_HISTORY - 1))
        {
            last_data_x = m_hscale * (MAX_WATCH_HISTORY + 1);
            painter.setPen(penText);
            painter.drawText(last_data_x, y1 - 2, text);
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

    // If the cursors are linked together, draw the link line
    if (m_linked && m_cursors2x.count() >= 2)
    {
        uint x1 = m_cursors2x.at(0) * m_hscale / 2.0;
        uint x2 = m_cursors2x.at(1) * m_hscale / 2.0;
        uint y = r.bottom() - 1 * m_fontheight;
        painter.drawLine(x1, y, x2, y);
    }

    for (int i=0; i<m_cursors2x.count(); i++)
    {
        uint cursorX = m_cursors2x.at(i);
        uint x = cursorX * m_hscale / 2.0;
        uint y = r.bottom() - i * m_fontheight;
        painter.drawLine(x, r.top(), x, y);

        QString text = QString::number(cursorX / 2 + hstart);
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

void WidgetWaveform::onLinked(bool isLinked)
{
    if (isLinked && m_cursors2x.count() >= 2)
        m_linked = int(m_cursors2x.at(1)) - int(m_cursors2x.at(0));
    else
        m_linked = 0;
    update();
}

QSize WidgetWaveform::sizeHint() const
{
    // The total data size plus some, to give some extra space on the right end
    uint extra = MAX_WATCH_HISTORY >> 4;
    QSize size((MAX_WATCH_HISTORY + extra) * m_hscale, 0);
    return size;
}

//============================================================================
// Mouse tracking
//============================================================================

void WidgetWaveform::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();
    if (!m_mousePressed)
        return;
    if(m_cursormoving) // User is moving the cursor
    {
        int mouse_in_dataX = m_mousePos.x() / (m_hscale / 2);
        m_cursors2x[m_cursor] = qBound(0, mouse_in_dataX, MAX_WATCH_HISTORY * 2);

        // If the first two cursors are linked together, move them both
        if (m_linked && m_cursors2x.count() >= 2)
        {
            if (m_cursor == 0) m_cursors2x[1] = qBound(0, int(m_cursors2x[0] + m_linked), MAX_WATCH_HISTORY * 2);
            if (m_cursor == 1) m_cursors2x[0] = qBound(0, int(m_cursors2x[1] - m_linked), MAX_WATCH_HISTORY * 2);
        }
        Q_ASSERT(m_cursors2x.count() >= 2);
        emit setLink(abs(int(m_cursors2x[0] / 2) - int(m_cursors2x[1] / 2)));

        setCursor(Qt::SizeHorCursor);
        update();
    }
    else // User is moving the pane and it needs to be scrolled
    {
        int deltaX = m_pinMousePos.x() - event->x();
        setCursor(Qt::ClosedHandCursor);
        emit scroll(deltaX);
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
        {
            m_cursor = (r.bottom() - m_mousePos.y()) / m_fontheight;
            m_cursormoving = true;
        }
        else
        // Next, try to find the cursor that is close to the mouse pointer (off by a few pixels)
        // If found, make that cursor active and ready to move it.
        for (int i=0; i<m_cursors2x.count(); i++)
        {
            int data_to_screenX = m_cursors2x.at(i) * (m_hscale / 2);
            if (abs(m_mousePos.x() - data_to_screenX) < 10)
            {
                m_cursor = i;
                m_cursormoving = true;
            }
        }
        if (m_cursormoving)
        {
            // If we identified a cursor, set it's new X coordiate
            int mouse_in_dataX = m_mousePos.x() / (m_hscale / 2);
            m_cursors2x[m_cursor] = qBound(0, mouse_in_dataX, MAX_WATCH_HISTORY * 2);
        }
    }
    setCursor(m_cursormoving ? Qt::SizeHorCursor : Qt::OpenHandCursor);
    update();
}

void WidgetWaveform::mouseReleaseEvent (QMouseEvent *)
{
    setCursor(Qt::ArrowCursor);
    m_mousePressed = false;
    m_cursormoving = false;
}

/*
 * Double clicking the mouse on the pane brings the current cursor to that point
 */
void WidgetWaveform::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_cursors2x.count())
    {
        int mouse_in_dataX = event->x() / (m_hscale / 2);
        m_cursors2x[m_cursor] = qBound(0, mouse_in_dataX, MAX_WATCH_HISTORY * 2);

        // If the first two cursors are linked together, move them both
        if (m_linked && m_cursors2x.count() >= 2)
        {
            if (m_cursor == 0) m_cursors2x[1] = qBound(0, int(m_cursors2x[0] + m_linked), MAX_WATCH_HISTORY * 2);
            if (m_cursor == 1) m_cursors2x[0] = qBound(0, int(m_cursors2x[1] - m_linked), MAX_WATCH_HISTORY * 2);
        }
        Q_ASSERT(m_cursors2x.count() >= 2);
        emit setLink(abs(int(m_cursors2x[0] / 2) - int(m_cursors2x[1] / 2)));

        update();
    }
}

void WidgetWaveform::onZoom(bool isUp)
{
    if (isUp)
        m_hscale *= 1.2;
    else
        m_hscale /= 1.2;
    m_hscale = qBound(1.0, m_hscale, 100.0);
    updateGeometry();
}

void WidgetWaveform::wheelEvent(QWheelEvent *event)
{
    onZoom(event->delta() > 0);
}

void WidgetWaveform::leaveEvent(QEvent *)
{
    setCursor(Qt::ArrowCursor);
    m_mousePressed = false;
    m_cursormoving = false;
}
