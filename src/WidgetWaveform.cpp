#include "ClassController.h"
#include "WidgetWaveform.h"
#include <QPaintEvent>
#include <QPainter>
#include <QSettings>
#include <QStringBuilder>
#include <QtGlobal>

WidgetWaveform::WidgetWaveform(QWidget *parent) : QWidget(parent)
{
    // Refresh graph when running simulation and when stopped
    connect(&::controller, &ClassController::onRunHeartbeat, this, [this](){ update(); });
    connect(&::controller, &ClassController::onRunStopped, this, &WidgetWaveform::onRunStopped);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setMouseTracking(true);
}

void WidgetWaveform::init(DockWaveform *dock, QString sid)
{
    // Sets a pointer to our UI parent dock
    m_dock = dock;
    setWhatsThis(sid);

    // Set up the two initial cursors and restore the form size
    QSettings settings;
    m_cursors2x.append(settings.value("dockWaveCursor1-" + sid, 1).toInt());
    m_cursors2x.append(settings.value("dockWaveCursor2-" + sid, 10).toInt());
    m_cursor = settings.value("dockWaveCursor-" + sid, 0).toInt();
    m_dY = settings.value("dockWaveHeight-" + sid, 20).toInt();
    onEnlarge(0);

    // Redraw the view when the vertical scrolling stops, this fixes the cursor corruption on vertical scroll
    connect(dock, &DockWaveform::verticalScrollStopped, this, [=]() { update(); });
}

WidgetWaveform::~WidgetWaveform()
{
    QSettings settings;
    for (uint i = 0; i < m_cursors2x.count(); i++)
        settings.setValue(QString("dockWaveCursor%1-%2").arg(i+1).arg(whatsThis()), m_cursors2x.at(i));
    settings.setValue("dockWaveCursor-" + whatsThis(), m_cursor);
}

void WidgetWaveform::paintEvent(QPaintEvent *pe)
{
    const QRect &r = pe->rect();
    QPainter painter(this);

    QFont font = painter.font();
    font.setPixelSize(m_fontheight);
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
    uint y = m_dY - 2; // Initial Y coordinate, the bottom of the first net
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
        y += m_dY; // Advance to the next Y coordinate, this is the height of each net
        vi = m_dock->getNext(it);
    }
    drawCursors(painter, r, hstart);
}

void WidgetWaveform::drawOneSignal_Net(QPainter &painter, uint y, uint hstart, watch *w, viewitem *viewitem)
{
    const uint wh[4] { 0, m_waveheight, m_waveheight / 2, 0 };
    static const QPen penHiZ = QPen(QColor(Qt::white), 1, Qt::DotLine);
    QColor fillColor = viewitem->color;
    fillColor.setAlphaF(0.5); // 50% intensity for fills
    QBrush stripeBrush(fillColor, Qt::Dense4Pattern); // Create striped brush pattern
    painter.setPen(viewitem->color);
    net_t data_cur, data_prev = ::controller.getWatch().at(w, hstart);
    for (int i = 0; i < MAX_WATCH_HISTORY; i++, data_prev = data_cur)
    {
        data_cur = ::controller.getWatch().at(w, hstart + i);
        if (data_cur > 2) // If the data is undef at this cycle, do not draw it
            continue;
        uint x1 = i * m_hscale;
        uint x2 = (i + 1) * m_hscale;
        uint y1 = wh[data_prev & 3];
        uint y2 = wh[data_cur & 3];

        switch (viewitem->format)
        {
            case ClassController::FormatNet::Logic: // Draw simple line logic
            case ClassController::FormatNet::Logic0Filled:
            case ClassController::FormatNet::Logic1Filled:
            {
                bool is_filled = ((data_cur == 0) && (viewitem->format == ClassController::FormatNet::Logic0Filled))
                              || ((data_cur == 1) && (viewitem->format == ClassController::FormatNet::Logic1Filled));
                if (Q_UNLIKELY(data_cur == 2)) // Hi-Z states use special color + dotted lines
                    painter.setPen(penHiZ);
                else if (is_filled)
                {
                    painter.setPen(Qt::NoPen); // Remove border
                    painter.fillRect(x1, y - m_waveheight, x2 - x1, m_waveheight, stripeBrush);
                    painter.setPen(viewitem->color); // Restore pen for lines
                }
                if (data_prev != data_cur)
                {
                    painter.drawLine(x1, y - y1, x1, y - y2);
                    painter.drawLine(x1, y - y2, x2, y - y2);
                }
                painter.drawLine(x1, y - y2, x2, y - y2);
                if (Q_UNLIKELY(data_cur == 2))
                    painter.setPen(viewitem->color);
            } break;
            case ClassController::FormatNet::TransAny: // Draw transition triangles
            case ClassController::FormatNet::TransUp:
            case ClassController::FormatNet::TransDown:
            {
                bool is_up = data_prev < data_cur;
                uint d = 1 + m_hscale / 5; // Stretch triangles as the scale moves up
                QBrush brush = painter.brush();
                painter.setBrush(QBrush(viewitem->color));
                if (!is_up && (viewitem->format != ClassController::FormatNet::TransUp)) // TransDown or TransAny
                {
                    QPoint shape[3] = {QPoint(x1 - d,y - y1), QPoint(x1,y), QPoint(x1 + d,y - y1)};
                    painter.drawPolygon(shape, 3);
                }
                if (is_up && (viewitem->format != ClassController::FormatNet::TransDown)) // TransUp or TransAny
                {
                    QPoint shape[3] = {QPoint(x1 - d,y), QPoint(x1,y - y2), QPoint(x1 + d,y)};
                    painter.drawPolygon(shape, 3);
                }
                painter.setBrush(brush);
            } break;
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
    QString text = ::controller.formatBus(viewitem->format, data_prev, width, m_decorated);
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
            text = ::controller.formatBus(viewitem->format, data_cur, width, m_decorated);
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
    QPen pen(Qt::yellow);
    painter.setPen(pen);
    uint bottom = r.bottom() - 2; // Lift up the cursors a little bit

    // If the cursors are linked together, draw the link line
    if (m_linked && (m_cursors2x.count() >= 2))
    {
        uint x1 = m_cursors2x.at(0) * m_hscale / 2.0;
        uint x2 = m_cursors2x.at(1) * m_hscale / 2.0;
        uint y = bottom - 1 * m_fontheight + 1;
        painter.drawLine(x1, y, x2, y);
    }

    for (int i=0; i<m_cursors2x.count(); i++)
    {
        uint cursorX = m_cursors2x.at(i);
        uint x = cursorX * m_hscale / 2.0;
        uint y = bottom - i * m_fontheight;
        painter.drawLine(x, r.top(), x, y + 1);

        QString text = QString::number(cursorX / 2 + hstart);
        QRect bb = painter.fontMetrics().boundingRect(text);
        bb.adjust(x, y, x, y);

        painter.drawText(x + 2, bottom - i * m_fontheight, text);
        painter.drawRect(bb.adjusted(0, 2, 4, -2));
    }

    // Draw the selected cursor on top
    if (m_cursor < uint(m_cursors2x.count()))
    {
        pen.setWidth(3);
        painter.setPen(pen);
        uint x = m_cursors2x[m_cursor] * m_hscale / 2.0;
        painter.drawLine(x, r.top(), x, bottom - m_cursor * m_fontheight + 1);

        emit cursorChanged(m_cursors2x[m_cursor] / 2 + hstart);
    }
}

/*
 * When a simulation run stops, check the (primary) cursor against the new sim cycle and scroll the view if the
 * cursor was positioned at the last cycle. This will work for short runs of 1 and 2 half-cycles, for which we
 * want to observe the signal traces step by step anyways. All other runs do not scroll the view.
 */
void WidgetWaveform::onRunStopped()
{
    if (m_cursors2x.count())
    {
        const uint hcycle = ::controller.getSimZ80().getCurrentHCycle() - 1;
        uint cx = m_cursors2x[m_cursor] / 2;
        int delta = hcycle - cx;
        if ((delta == 1) || (delta == 2)) // Cursor "captures" the waveform when it differs from the hcycle by 1 or two
        {
            m_cursors2x[m_cursor] = hcycle * 2 + 1; // Move the cursor to the new hcycle edge
            emit scroll(m_hscale * delta);

            // If the first two cursors are linked together, move them both
            if (m_linked && (m_cursors2x.count() >= 2))
            {
                if (m_cursor == 0) m_cursors2x[1] = qBound(0, int(m_cursors2x[0] + m_linked), MAX_WATCH_HISTORY * 2);
                if (m_cursor == 1) m_cursors2x[0] = qBound(0, int(m_cursors2x[1] - m_linked), MAX_WATCH_HISTORY * 2);
            }
            emit setLink(abs(int(m_cursors2x[0] / 2) - int(m_cursors2x[1] / 2)));
        }
    }
    update();
}

void WidgetWaveform::onLinked(bool isLinked)
{
    if (isLinked && (m_cursors2x.count() >= 2))
        m_linked = int(m_cursors2x.at(1)) - int(m_cursors2x.at(0));
    else
        m_linked = 0;
    update();
}

void WidgetWaveform::onDecorated(bool isDecorated)
{
    m_decorated = isDecorated;
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
        setCursorsPos(m_cursor, mouse_in_dataX);
        setCursor(Qt::SizeHorCursor);
        emit cursorPosChanged(m_cursor, m_cursors2x[m_cursor]); // Emit cursor position for sync
    }
    else // User is scrolling the pane
    {
        int deltaX = m_pinMousePos.x() - event->position().x();
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
    int mouse_in_dataX = event->position().x() / (m_hscale / 2);
    setCursorsPos(m_cursor, mouse_in_dataX);
    emit cursorPosChanged(m_cursor, m_cursors2x[m_cursor]); // Emit cursor position for sync
}

void WidgetWaveform::setCursorsPos(uint index, uint pos)
{
    // Update cursor position if valid
    if (index < m_cursors2x.count())
    {
        m_cursor = index;
        m_cursors2x[index] = qBound(0, int(pos), MAX_WATCH_HISTORY * 2);

        // Handle linked cursors
        if (m_linked && (m_cursors2x.count() >= 2))
        {
            if (m_cursor == 0) m_cursors2x[1] = qBound(0, int(m_cursors2x[0] + m_linked), MAX_WATCH_HISTORY * 2);
            if (m_cursor == 1) m_cursors2x[0] = qBound(0, int(m_cursors2x[1] - m_linked), MAX_WATCH_HISTORY * 2);
        }
        emit setLink(abs(int(m_cursors2x[0] / 2) - int(m_cursors2x[1] / 2))); // Emit link delta value
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

void WidgetWaveform::onEnlarge(int delta)
{
    m_dY = qBound<int>(10, m_dY + delta, 50);
    m_waveheight = 0.8 * m_dY;
    m_fontheight = 0.7 * m_dY;
    update();
}

void WidgetWaveform::leaveEvent(QEvent *)
{
    setCursor(Qt::ArrowCursor);
    m_mousePressed = false;
    m_cursormoving = false;
}
