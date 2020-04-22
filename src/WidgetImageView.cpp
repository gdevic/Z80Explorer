#include "WidgetImageView.h"
#include "ClassChip.h"
#include "ClassController.h"
#include "ClassSimX.h"
#include "DialogEditAnnotations.h"
#include "WidgetImageOverlay.h"

#include <QDebug>
#include <QInputDialog>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QTimer>

WidgetImageView::WidgetImageView(QWidget *parent) :
    QWidget(parent),
    m_image(QImage()),
    m_highlight_segment(nullptr),
    m_highlight_box(nullptr)
{
    setZoomMode(Fit);
    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(QCursor(Qt::CrossCursor));

    // Connect the view's internal intent to move its image (for example, when the user drags it with a mouse)
    connect(this, SIGNAL(imageMoved(QPointF)), this, SLOT(moveBy(QPointF)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(contextMenu(const QPoint&)));

    // Create and set the image overlay widget
    m_ov = new WidgetImageOverlay(this);
    m_ov->setParent(this);
    m_ov->move(10, 10);
    m_ov->show();

    // Create a timer that updates image every 1/2 seconds to show highlight blink
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    m_timer->start();

    connect(this, SIGNAL(pointerData(int,int)), m_ov, SLOT(onPointerData(int,int)));
    connect(this, SIGNAL(clearPointerData()), m_ov, SLOT(onClearPointerData()));
    connect(m_ov, SIGNAL(actionCoords()), this, SLOT(onCoords()));
    connect(m_ov, SIGNAL(actionFind(QString)), this, SLOT(onFind(QString)));

    connect(&::controller, SIGNAL(onRunStopped(uint)), this, SLOT(onRunStopped(uint)));
}

/*
 * Init function called after all the images have been loaded so we can prepare the initial view
 */
void WidgetImageView::init()
{
    m_image = ::controller.getChip().getImage(0);
    m_ov->setText(3, m_image.text("name"));
    m_ov->setLayerNames(::controller.getChip().getLayerNames());
    m_scale = 0.19; // Arbitrary initial scaling.. looks perfect on my monitor ;-)
    setZoomMode(Value);
}

/*
 * Timer timeout handler
 */
void WidgetImageView::onTimeout()
{
    if (m_timer_tick)
        m_timer_tick--;
    update();
}

/*
 * Controller signals us that the current simulation run completed
 */
void WidgetImageView::onRunStopped(uint)
{
    update();
}

void WidgetImageView::setZoomMode(ZoomType mode)
{
    qreal sx = (qreal) width()/m_image.width();
    qreal sy = (qreal) height()/m_image.height();

    m_view_mode = mode;
    switch(m_view_mode)
    {
    case Fit:
        m_tex = QPointF(0.5, 0.5); // Map texture center to view center
        m_scale = sx > sy ? sy : sx;
        if(sx>1.0 && sy>1.0)
            m_scale = 1.0;
        break;

    case Fill:
        m_tex = QPointF(0.5, 0.5); // Map texture center to view center
        m_scale = sx > sy ? sx : sy;
        if(sx>1.0 && sy>1.0)
            m_scale = 1.0;
        break;

    case Identity: // 1:1 Zoom ratio
        m_scale = 1.0;
        break;

    default: // Any other value is a real number zoom ratio
        break;
    }

    calcTransform();
    update();
    setFocus();
}

void WidgetImageView::setZoom(double value)
{
    // Make sure that the zoom value is in the sane range
    m_scale = qBound(0.1, value, 10.0);
    setZoomMode(Value);
}

void WidgetImageView::moveBy(QPointF delta)
{
    m_tex -= delta;
    clampImageCoords(m_tex);
    update();
}

void WidgetImageView::moveTo(QPointF pos)
{
    m_tex = pos;
    clampImageCoords(m_tex);
    update();
}

/*
 * Open coordinate dialog and center image on user input coordinates
 */
void WidgetImageView::onCoords()
{
    bool ok;
    QString coords = QInputDialog::getText(this, "Center Image", "Enter the coordinates x,y", QLineEdit::Normal, "", &ok);
    if (ok && !coords.isEmpty())
    {
        QRegularExpression re("(\\d+)\\s*,\\s*(\\d+)");
        QRegularExpressionMatch match = re.match(coords);
        if (match.hasMatch())
        {
            int x = match.captured(1).toInt();
            int y = match.captured(2).toInt();
            moveTo(QPointF(qreal(x) / m_image.width(), qreal(y) / m_image.height()));
        }
    }
}

// Clamp the image coordinates into the range [0,1]
void WidgetImageView::clampImageCoords(QPointF &tex, qreal xmax, qreal ymax)
{
    tex.setX(qBound(0.0, tex.x(), xmax));
    tex.setY(qBound(0.0, tex.y(), ymax));
}

#if 0
// Return the coordinates within the image that the view is clipped at, given
// its position and zoom ratio. This rectangle corresponds to what the user sees.
QRectF WidgetImageView::getImageView()
{
    // Point 0 is at the top-left corner; point 1 is at the bottom-right corner of the view.
    // Do the inverse map to get to the coordinates in the texture space.
    QPointF t0 = m_invtx.map(QPointF(0,0));
    QPointF t1 = m_invtx.map(QPointF(m_viewPort.right(), m_viewPort.bottom()));

    // Normalize the texture coordinates and clamp them to the range of [0,1]
    t0.rx() /= m_image.width();
    t0.ry() /= m_image.height();
    t1.rx() /= m_image.width();
    t1.ry() /= m_image.height();

    clampImageCoords(t0);
    clampImageCoords(t1);

    return QRectF(t0, t1);
}
#endif

//============================================================================
// Callbacks
//============================================================================

void WidgetImageView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    m_viewPort = painter.viewport();

    // Point 0 is at the top-left corner; point 1 is at the bottom-right corner of the view.
    // Do the inverse map to get to the coordinates in the texture space.
    QPointF t0 = m_invtx.map(QPoint(0,0));
    QPointF t1 = m_invtx.map(QPoint(m_viewPort.right(), m_viewPort.bottom()));
    clampImageCoords(t0, m_image.width() - 1, m_image.height() - 1);
    clampImageCoords(t1, m_image.width() - 1, m_image.height() - 1);
    m_imageView = QRectF(t0, t1);

    // Transformation allows us to simply draw as if the source and target are the same size
    calcTransform();
    QRect size(0, 0, m_image.width(), m_image.height());

    painter.setTransform(m_tx);
    painter.translate(-0.5, -0.5); // Adjust for Qt's very precise rendering
    painter.drawImage(size, m_image, size);

    //------------------------------------------------------------------------
    // Base image is "vss.vcc.nets" with all the nets drawn as inactive
    // This method is faster since we only draw active nets (over the base
    // image which already has all the nets pre-drawn as inactive)
    //------------------------------------------------------------------------
    if (m_drawActiveNets)
    {
        painter.save();
        painter.setPen(QPen(Qt::black, 1, Qt::SolidLine));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        painter.setBrush(QColor(255, 0, 255));
        for (uint i=3; i<::controller.getSimx().getNetlistCount(); i++)
        {
            if (::controller.getSimx().getNetState(i) == 1)
            {
                for (auto path : ::controller.getChip().getSegment(i)->paths)
                    painter.drawPath(path);
            }
        }
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Draw two optional features on top of the image: a box (a transistor) and
    // a segment (a signal), both of which were selected via the "Find" dialog
    //------------------------------------------------------------------------
    {
        painter.save();
        painter.setPen(QPen(QColor(), 0, Qt::NoPen)); // No outlines
        if (m_timer_tick & 1)
        {
            painter.setBrush(QColor(255,255,0));
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
        }
        else
        {
            painter.setBrush(QColor(100,200,200));
            painter.setCompositionMode(QPainter::CompositionMode_Plus);
        }
        if (m_highlight_box)
            painter.drawRect(*m_highlight_box);
        if (m_highlight_segment)
        {
            for (auto path : m_highlight_segment->paths)
                painter.drawPath(path);
        }
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Draw transistors
    //------------------------------------------------------------------------
    if (m_drawTransistors)
    {
        painter.save();
        ::controller.getChip().expDrawTransistors(painter);
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Draw text annotations
    //------------------------------------------------------------------------
    if (m_drawAnnotations)
    {
        painter.save();
        ::controller.getChip().annotate.draw(painter, m_scale);
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Draw the mouse selected area
    //------------------------------------------------------------------------
    if (m_drawSelection)
    {
        painter.save();
        painter.setPen(QPen(Qt::white, 3.0 / m_scale, Qt::DashLine));
        painter.drawRect(m_areaRect);
        painter.restore();
    }
}

void WidgetImageView::calcTransform()
{
    int sx = m_image.width();
    int sy = m_image.height();

    QTransform mtr1(1, 0, 0, 1, -sx * m_tex.x(), -sy * m_tex.y());
    QTransform msc1(m_scale, 0, 0, m_scale, 0, 0);
    QTransform mtr2(1, 0, 0, 1, m_viewPort.width()/2, m_viewPort.height()/2);

    m_tx = mtr1 * msc1 * mtr2;
    m_invtx = m_tx.inverted();
}

void WidgetImageView::resizeEvent(QResizeEvent * event)
{
    m_panelSize = event->size();
}

//============================================================================
// Mouse tracking and keyboard input
//============================================================================

void WidgetImageView::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();

    if(m_mouseLeftPressed) // With a left mouse button pressed: pan (move) the image
    {
        // This is beautiful.
        double dX = m_mousePos.x() - m_pinMousePos.x();
        double dY = m_mousePos.y() - m_pinMousePos.y();
        m_pinMousePos = m_mousePos;

        dX = dX / m_image.width();
        dY = dY / m_image.height();

        emit imageMoved(QPointF(dX/m_scale, dY/m_scale));
    }
    else if (m_mouseRightPressed) // With a right mouse button pressed: define the selection area
    {
        QPoint pos1 = m_invtx.map(m_pinMousePos);
        QPoint pos2 = m_invtx.map(event->pos());
        m_areaRect.setTopLeft(pos1);
        m_areaRect.setBottomRight(pos2);

        update();
    }
    else
    {
        // With no buttons pushed, a mouse move needs to update the information
        // about the net or object it is currently pointing to
        QPoint imageCoords = m_invtx.map(event->pos());
        if (m_image.valid(imageCoords.x(), imageCoords.y()))
        {
            emit pointerData(imageCoords.x(), imageCoords.y());

            QList<int> nodes = ::controller.getChip().getNodesAt(imageCoords.x(), imageCoords.y());
            QString s;
            for (int &i : nodes)
                s.append(QString::number(i)).append(',');
            QStringList trans = ::controller.getChip().getTransistorsAt(imageCoords.x(), imageCoords.y());
            for (QString name : trans)
                s.append(name).append(',');
            m_ov->setText(1, s);

            // For each node number in the nodes list, get their name
            QList<QString> list;
            for(net_t n : nodes)
            {
                QString name = ::controller.getNetlist().get(n);
                if (!name.isEmpty() && !list.contains(name)) // Do not create duplicates
                    list.append(name);
            }
            m_ov->setText(2, list.join(','));
        }
        else
        {
            // Oops - the pointer is in this widget, but it's not currently over
            // the image, so we have no data to report.
            emit clearPointerData();
        }
    }
}

/*
 * User pressed a mouse button; the right button context menu is handled via customContextMenuRequested() signal
 */
void WidgetImageView::mousePressEvent(QMouseEvent *event)
{
    m_pinMousePos = event->pos();
    m_mouseLeftPressed = event->button() == Qt::LeftButton;
    m_mouseRightPressed = event->button() == Qt::RightButton;
    m_drawSelection = m_mouseRightPressed;
}

void WidgetImageView::mouseReleaseEvent (QMouseEvent *event)
{
    m_mouseLeftPressed = false;
    m_mouseRightPressed = false;
    m_drawSelection = event->button() == Qt::RightButton;
}

void WidgetImageView::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0)
        m_scale = m_scale * 1.2;
    else
        m_scale = m_scale / 1.2;
    emit setZoom(m_scale);
}

void WidgetImageView::leaveEvent(QEvent *)
{
    emit clearPointerData();
}

void WidgetImageView::keyPressEvent(QKeyEvent *event)
{
    // Approximate image move offsets in the texture space
    qreal dx = qreal(m_image.width()) / (m_viewPort.width() * m_scale * 100);
    qreal dy = qreal(m_image.height()) / (m_viewPort.height() * m_scale * 200);
    bool alt = event->modifiers() & Qt::AltModifier;
    int i = -1;

    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9)
        i = event->key() - Qt::Key_1;
    else if (event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z)
        i = event->key() - Qt::Key_A + 9;
    else
    switch (event->key())
    {
    case Qt::Key_F1:
        switch(m_view_mode)
        {
            case Fit: setZoomMode(Fill); break;
            case Fill: setZoomMode(Identity); break;
            case Identity: setZoomMode(Fit); break;
            case Value: setZoomMode(Fit); break;
        }
        break;
    case Qt::Key_Space: m_drawActiveNets = !m_drawActiveNets; break;
    case Qt::Key_Comma: m_drawAnnotations = !m_drawAnnotations; break;
    case Qt::Key_Period: m_drawTransistors = !m_drawTransistors; break;
    case Qt::Key_Left: m_tex += QPointF(-dx,0); break;
    case Qt::Key_Right: m_tex += QPointF(dx,0); break;
    case Qt::Key_Up: m_tex += QPointF(0,-dy); break;
    case Qt::Key_Down: m_tex += QPointF(0,dy); break;
    case Qt::Key_PageUp: setZoom(m_scale * 1.2); break;
    case Qt::Key_PageDown: setZoom(m_scale / 1.2); break;
    }

    if (i >= 0)
    {
        if (alt) // Compositing multiple images view
        {
            QPainter painter(&m_image);
            painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
            painter.drawImage(0,0, ::controller.getChip().getImage(i));
            painter.end();
        }
        else // Simple image view
        {
            m_image = ::controller.getChip().getImage(i); // Creates a shallow image copy
            m_ov->setText(3, m_image.text("name"));
        }
    }
    update();
}

/*
 * Context menu handler, called when the user right-clicks somewhere on the image view
 */
void WidgetImageView::contextMenu(const QPoint& pos)
{
    QMenu contextMenu(this);

    // "Add annotation" option only if the selection area has some width to it
    QAction actionAddAnnotation("Add annotation...", this);
    connect(&actionAddAnnotation, SIGNAL(triggered()), this, SLOT(addAnnotation()));
    if (m_drawSelection && m_areaRect.width())
        contextMenu.addAction(&actionAddAnnotation);

    QAction actionEditAnnotation("Edit annotations...", this);
    connect(&actionEditAnnotation, SIGNAL(triggered()), this, SLOT(editAnnotations()));
    contextMenu.addAction(&actionEditAnnotation);

    contextMenu.exec(mapToGlobal(pos));

    m_drawSelection = false;
    m_pinMousePos = QPoint();
    m_areaRect.setRect(0,0,0,0);
}

/*
 * // Adds a new annotation within the selected box and opens dialog to edit it
 */
void WidgetImageView::addAnnotation()
{   
    bool ok;
    QString text = QInputDialog::getText(this, "Add annotation", "Annotation text:", QLineEdit::Normal, "", &ok);
    if (ok && !text.isEmpty())
    {
        ::controller.getChip().annotate.add(text, m_areaRect.normalized());
        DialogEditAnnotations dlg(this);
        dlg.exec();
    }
}
/*
 * Opens dialog to edit annotations
 */
void WidgetImageView::editAnnotations()
{
    DialogEditAnnotations dlg(this);
    dlg.exec();
}

/*
 * Search for the named feature
 * This function is called when the user enters some text in the Find box
 * Typing [Enter] in the empty Find dialog will re-trigger the highlight flash
 * Typing "0" will clear all highlights
 */
void WidgetImageView::onFind(QString text)
{
    if (text.length() == 0)
    {
        m_timer_tick = 10;
        return;
    }
    m_timer_tick = 0; // Stop highlights flash
    if (text=='0')
    {
        m_highlight_box = nullptr;
        m_highlight_segment = nullptr;
        qDebug() << "Highlights cleared";
        return;
    }
    if (text.startsWith(QChar('t')) && (text.length() > 3)) // Search the transistors by their number, which has at least 3 digits
    {
        const transdef *trans = ::controller.getChip().getTrans(text);
        if (trans)
        {
            m_highlight_box = &trans->box;
            qDebug() << "Found transistor" << text;
            qDebug() << trans->box << "(x,y):" << trans->box.x() << "," << trans->box.y() << "w:" << trans->box.width() << "h:" << trans->box.height();
            m_timer_tick = 10;
            update();
        }
        else
            qWarning() << "Segment" << text << "not found";
    }
    else // Search the visual segment numbers or net names
    {
        bool ok;
        net_t nodenum = text.toUInt(&ok);
        if (!ok) // Check if the input is a net name
        {
            nodenum = ::controller.getNetlist().get(text);
            ok = nodenum > 0;
        }
        if (ok)
        {
            const segdef *seg = ::controller.getChip().getSegment(nodenum);
            if (seg->nodenum)
            {
                m_highlight_segment = seg;
                qDebug() << "Found segment" << text;
                qDebug() << "Path:" << seg->paths.count();
                m_timer_tick = 10;
                update();
            }
            else
                qWarning() << "Segment" << text << "not found";
        }
        else
            qWarning() << "Invalid input value" << text;
    }
}
