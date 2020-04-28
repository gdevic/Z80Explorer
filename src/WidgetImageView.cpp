#include "WidgetImageView.h"
#include "ClassChip.h"
#include "ClassController.h"
#include "ClassSimX.h"
#include "DialogEditAnnotations.h"
#include "WidgetImageOverlay.h"

#include <QDebug>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QTimer>
#include <QToolTip>

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

//============================================================================
// Callbacks and events
//============================================================================

void WidgetImageView::paintEvent(QPaintEvent *)
{
    // Measure the drawing performance
    QElapsedTimer timer;
    timer.start();

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
                for (const auto &path : ::controller.getChip().getSegment(i)->paths)
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
            for (const auto &path : m_highlight_segment->paths)
                painter.drawPath(path);
        }
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Draw nodes picked by the mouse double-click and then expanded
    //------------------------------------------------------------------------
    if (m_drivingNets.count())
    {
        painter.save();
        painter.setPen(QPen(QColor(), 0, Qt::NoPen)); // No outlines
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        QColor col(Qt::darkBlue); // The starting color of expanded (not the first) nets
        int h, s, l; // We are using hue and saturation of the starting color, varying the lightness
        col.getHsl(&h, &s, &l);
        l = 0; // Set the lightness component of the starting color to 0

        for (int i = 0; i < m_drivingNets.count(); i++)
        {
            net_t net = m_drivingNets[i];

            // Coloring heuristic:
            // 1. The very first net has a distinct color since that's our base net
            // 2. Each successive net is colored in the increasing brightness of another color (blue)
            // 3. Except for nets that have defined custom colors (like clk, for example)
            if (i == 0)
                painter.setBrush(QColor(255,255,255)); // The color of the first net
            else
            {
                if (::controller.getColors().isDefined(net))
                    painter.setBrush(::controller.getColors().get(net));
                else
                {
                    // Proportionally increment the lightness component to visually separate different nets
                    col.setHsl(h, s, l + 127);
                    painter.setBrush(col);
                    if (m_drivingNets.count() > 1) // Precompute the lightness for the next net
                        l += 128 / (m_drivingNets.count() - 1);
                }
            }

            const segdef *seg = ::controller.getChip().getSegment(net);
            for (const auto &path : seg->paths)
                painter.drawPath(path);
        }
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Draw transistors
    //------------------------------------------------------------------------
    if (m_drawActiveTransistors || m_drawAllTransistors)
    {
        painter.save();
        ::controller.getChip().expDrawTransistors(painter, m_imageView.toAlignedRect(), m_drawAllTransistors);
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Dynamically write nearby net names (experimental)
    //------------------------------------------------------------------------
    {
        painter.save();
        ::controller.getChip().expDynamicallyNameNets(painter, m_imageView.toAlignedRect(), m_scale);
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

    // Measure the drawing performance
    qreal ms = timer.elapsed();
    if (ms > 200) // If the drawing takes more than this many milliseconds, it will be measured and shown
    {
        m_perf.enqueue(ms);
        ms = 0; for (int i = 0; i < m_perf.count(); i++) ms += m_perf.at(i);
        qDebug() << "Widget paint:" << qRound(ms / m_perf.count()) << "ms";
        if (m_perf.count() == 4) m_perf.dequeue();
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

void WidgetImageView::resizeEvent(QResizeEvent *event)
{
    m_panelSize = event->size();
}

/*
 * The widget main event handler, we want to catch the QEvent::ToolTip events
 */
bool WidgetImageView::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QPoint pos = m_invtx.map(m_mousePos);
        QVector<net_t> nets = ::controller.getChip().getNetsAt<false>(pos.x(), pos.y());
        if (nets.count() == 1)
        {
            QStringList tooltip { ::controller.getNetlist().get(nets[0]), ::controller.getChip().tips.get(nets[0]) };
            tooltip.removeAll({}); // Remove empty components (from the above, if none defined)
            QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
            QToolTip::showText(helpEvent->globalPos(), tooltip.join("<br>"));

            event->ignore();
            return true;
        }
        QToolTip::hideText();
        event->ignore();
    }
    return QWidget::event(event);
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
        // With no buttons pushed, update information on which nets or objects the mouse is pointing to
        QPoint imageCoords = m_invtx.map(event->pos());
        if (m_image.valid(imageCoords.x(), imageCoords.y()))
        {
            emit pointerData(imageCoords.x(), imageCoords.y());

            const QVector<net_t> nodes = ::controller.getChip().getNetsAt<true>(imageCoords.x(), imageCoords.y());
            QString s;
            for (const uint i : nodes)
                s.append(QString::number(i)).append(", ");
            QString trans = ::controller.getChip().getTransistorAt(imageCoords.x(), imageCoords.y());
            s.append(trans);
            m_ov->setText(1, s);

            // For each node number in the nodes list, get their name
            const QStringList list = ::controller.getNetlist().get(nodes);
            m_ov->setText(2, list.join(", "));

            // XXX Test more the usability of this tooltip
            //QToolTip::showText(event->globalPos(), list.join(","));
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

/*
 * Double-clicking the mouse on a point in the image selects a net to trace
 * Selecting a point with no valid nets clears the selection
 */
void WidgetImageView::mouseDoubleClickEvent (QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_mousePos = event->pos();
        QPoint imageCoords = m_invtx.map(event->pos());
        if (m_image.valid(imageCoords.x(), imageCoords.y()))
        {
            // Select only one valid net number that is not vss or vcc
            QVector<net_t> nets = ::controller.getChip().getNetsAt<false>(imageCoords.x(), imageCoords.y());
            if (nets.count() == 0) // No valid nets, clear the base selected net
                m_drivingNets.clear();
            if (nets.count() == 1) // One valid net, set it as the base selected net
                m_drivingNets = nets;
            update();
        }
    }
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
    case Qt::Key_Period: m_drawActiveTransistors = !m_drawActiveTransistors, m_drawAllTransistors = false; break;
    case Qt::Key_Greater: m_drawAllTransistors = !m_drawAllTransistors; break;
    case Qt::Key_Left: moveBy(QPointF(dx,0)); break;
    case Qt::Key_Right: moveBy(QPointF(-dx,0)); break;
    case Qt::Key_Up: moveBy(QPointF(0,dy)); break;
    case Qt::Key_Down: moveBy(QPointF(0,-dy)); break;
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

    // "Add annotation" option, only if the selection area has some width to it
    QAction actionAddAnnotation("Add annotation...", this);
    connect(&actionAddAnnotation, SIGNAL(triggered()), this, SLOT(addAnnotation()));
    if (m_drawSelection && m_areaRect.width())
        contextMenu.addAction(&actionAddAnnotation);

    // "Driving nets" option, only if the user picked a single net node
    QAction actionDriving("Driving nets", this);
    connect(&actionDriving, SIGNAL(triggered()), this, SLOT(netsDriving()));
    if (m_drivingNets.count() == 1)
        contextMenu.addAction(&actionDriving);

    // "Driven by" option, only if the user picked a single net node
    QAction actionDriven("Driven by", this);
    connect(&actionDriven, SIGNAL(triggered()), this, SLOT(netsDriven()));
    if (m_drivingNets.count() == 1)
        contextMenu.addAction(&actionDriven);

    // "Edit tip..." option, only if the user picked a single net node
    QAction actionEditTip("Edit tip...", this);
    connect(&actionEditTip, SIGNAL(triggered()), this, SLOT(editTip()));
    if (m_drivingNets.count() == 1)
        contextMenu.addAction(&actionEditTip);

    // "Edit net name..." option, only if the user picked a single net node
    QAction actionEditNetName("Edit net name...", this);
    connect(&actionEditNetName, SIGNAL(triggered()), this, SLOT(editNetName()));
    if (m_drivingNets.count() == 1)
        contextMenu.addAction(&actionEditNetName);

    QAction actionEditAnnotation("Edit annotations...", this);
    connect(&actionEditAnnotation, SIGNAL(triggered()), this, SLOT(editAnnotations()));
    contextMenu.addAction(&actionEditAnnotation);

    contextMenu.exec(mapToGlobal(pos));

    m_drawSelection = false;
    m_pinMousePos = QPoint();
    m_areaRect.setRect(0,0,0,0);
}

/*
 * Adds a new annotation within the selected box and opens dialog to edit it
 */
void WidgetImageView::addAnnotation()
{   
    bool ok;
    QString text = QInputDialog::getText(this, "Add annotation", "Annotation text:", QLineEdit::Normal, "", &ok);
    if (ok && !text.isEmpty())
    {
        ::controller.getChip().annotate.add(text, m_areaRect.normalized());
        DialogEditAnnotations dlg(this);
        QVector<uint> sel { ::controller.getChip().annotate.count() - 1 };
        dlg.selectRows(sel); // Selects the last annotation (the newly added one)
        dlg.exec();
    }
}

/*
 * Opens dialog to edit annotations
 */
void WidgetImageView::editAnnotations()
{
    DialogEditAnnotations dlg(this);
    QVector<uint> sel;
    QPoint pos = m_invtx.map(m_pinMousePos);
    if (m_areaRect.isEmpty())
        sel = ::controller.getChip().annotate.get(pos);
    else
        sel = ::controller.getChip().annotate.get(m_areaRect.normalized());
    dlg.selectRows(sel); // Selects annotations under the mouse pointer
    dlg.exec();
}

/*
 * Opens dialog to edit a tip
 */
void WidgetImageView::editTip()
{
    Q_ASSERT(m_drivingNets.count() == 1);
    net_t net = m_drivingNets[0];
    QString name = ::controller.getNetlist().get(net);
    QString oldTip = ::controller.getChip().tips.get(net);
    bool ok;
    QString tip = QInputDialog::getText(this, "Edit tip", QString("Enter the tip for the selected net %1 (%2)").arg(name).arg(QString::number(net)), QLineEdit::Normal, oldTip, &ok);
    if (ok)
        ::controller.getChip().tips.set(tip, net);
}

/*
 * Appends nets that the selected net is driving
 */
void WidgetImageView::netsDriving()
{
    Q_ASSERT(m_drivingNets.count() == 1);
    m_drivingNets.append(::controller.getNetlist().netsDriving(m_drivingNets[0]));
    QStringList list = ::controller.getNetlist().get(m_drivingNets);
    list.removeFirst(); // Remove the first element which is the net we started at
    qInfo() << QString::number(m_drivingNets[0]) << "driving" << list;
}

/*
 * Appends nets that drive the selected net
 */
void WidgetImageView::netsDriven()
{
    Q_ASSERT(m_drivingNets.count() == 1);
    m_drivingNets.append(::controller.getNetlist().netsDriven(m_drivingNets[0]));
    QStringList list = ::controller.getNetlist().get(m_drivingNets);
    list.removeFirst(); // Remove the first element which is the net we started at
    qInfo() << QString::number(m_drivingNets[0]) << "driven by" << list;
}

/*
 * Opens dialog to edit selected net name (alias)
 *
 * If the new name equals the old name (covers empty/empty and unchanged cases)
 *   - do nothing, return
 * If the new name already belongs to another net
 *   - ask the user to detach the name, if "No", return
 *   - delete the name from another net
 * If the selected net does not have a name
 *   - set new name to the selected net, return
 * If the selected net already has a name
 *   If the new name is empty (delete op)
 *     - ask the user to delete the name, if "No", return
 *     - delete the name for this net, return
 *   If the new name is not empty
 *     - rename this net, return
 */
void WidgetImageView::editNetName()
{
    Q_ASSERT(m_drivingNets.count() == 1);
    net_t newNet = m_drivingNets[0];
    QStringList allNames = ::controller.getNetlist().getNetnames();
    QString oldName = ::controller.getNetlist().get(newNet);
    bool ok;
    QString newName = QInputDialog::getText(this, "Edit net name", "Enter the name (alias) of the selected net " + QString::number(newNet) + "\n", QLineEdit::Normal, oldName, &ok);
    newName = newName.trimmed().toLower(); // Trim spaces to keep net names lowercased
    if (!ok || (newName == oldName))
        return;
    net_t otherNet = ::controller.getNetlist().get(newName);
    if (allNames.contains(newName))
    {
        if (QMessageBox::question(this, "Edit net name", "The name '" + newName + "' is already attached to another net.\nDo you want to continue (the other net will become nameless)?") != QMessageBox::Yes)
            return;
        ::controller.deleteNetName(otherNet);
    }
    if (oldName.isEmpty())
        ::controller.setNetName(newName, newNet);
    else
    {
        if (newName.isEmpty())
        {
            if (QMessageBox::question(this, "Edit net name", "Delete name '" + oldName + "' of the net " + QString::number(newNet) + " ?") != QMessageBox::Yes)
                return;
            ::controller.deleteNetName(newNet);
        }
        else
            ::controller.renameNet(newName, newNet);
    }
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
