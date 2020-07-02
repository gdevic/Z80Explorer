#include "WidgetImageView.h"
#include "ClassChip.h"
#include "ClassController.h"
#include "ClassSimZ80.h"
#include "DialogEditAnnotations.h"
#include "DialogSchematic.h"
#include "WidgetImageOverlay.h"
#include <QDebug>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QToolTip>

WidgetImageView::WidgetImageView(QWidget *parent) :
    QWidget(parent),
    m_image(QImage())
{
    setZoomMode(Fit);
    setMouseTracking(true);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(QCursor(Qt::CrossCursor));
    setAcceptDrops(true);

    // Create and set the image overlay widget
    m_ov = new WidgetImageOverlay(this);
    m_ov->setParent(this);
    m_ov->move(10, 10);
    m_ov->show();

    connect(m_ov, SIGNAL(actionCoords()), this, SLOT(onCoords()));
    connect(m_ov, SIGNAL(actionFind(QString)), this, SLOT(onFind(QString)));
    connect(m_ov, SIGNAL(actionSetImage(int)), this, SLOT(setImage(int)));
    // Map overlay buttons directly to our keyboard handler and pass the corresponding key commands
    connect(m_ov, &WidgetImageOverlay::actionButton, this, [this](int i)
    {
        static const int key[4] = { Qt::Key_X, Qt::Key_Space, Qt::Key_T, Qt::Key_L };
        QKeyEvent event(QEvent::None, key[i], Qt::NoModifier, 0, 0, 0);
        keyPressEvent(&event);
    });
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(contextMenu(const QPoint&)));
    // Called by the sim when the current run stops at a given half-cycle
    connect(&::controller, &ClassController::onRunStopped, this, [=]() { update(); });
    connect(&::controller, &ClassController::syncView, this, &WidgetImageView::syncView);

    connect(&m_timer, &QTimer::timeout, this, &WidgetImageView::onTimeout);
    m_timer.start(500);
}

/*
 * Init function called after all the images have been loaded so we can prepare the initial view
 */
void WidgetImageView::init()
{
    m_ov->setImageNames(::controller.getChip().getImageNames());
    setImage(1); // Display the second image (colored nets)
    m_scale = 0.19; // Arbitrary initial scaling.. looks perfect on my monitor ;-)
    setZoomMode(Value);
    m_enable_ctrl = true; // Now it is safe to enable Ctrl modifier key
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

void WidgetImageView::setZoom(qreal value)
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
    QString coords = QInputDialog::getText(this, "Center Image", "Enter the coordinates x,y",
                                           QLineEdit::Normal, "", &ok, Qt::MSWindowsFixedSizeDialogHint);
    if (ok && !coords.isEmpty())
    {
        QRegularExpression re( R"((\d+)\s*,\s*(\d+))" );
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

    // Avoid flicker by not drawing certain features if the mouse is selecting or moving the image
    bool mouseOff = !(::controller.isSimRunning() && (m_mouseRightPressed || m_mouseLeftPressed));
    //------------------------------------------------------------------------
    // Base image is "vss.vcc.nets" with all the nets drawn as inactive
    // This method is faster since we only draw active nets (over the base
    // image which already has all the nets pre-drawn as inactive)
    //------------------------------------------------------------------------
    if (m_drawActiveNets && mouseOff)
    {
        painter.save();
        painter.setPen(QPen(Qt::black, 1, Qt::SolidLine));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        painter.setBrush(QColor(255, 0, 255));
        for (uint i=3; i<::controller.getSimZ80().getNetlistCount(); i++)
        {
            if (::controller.getSimZ80().getNetState(i) == 1)
            {
                for (const auto &path : ::controller.getChip().getSegment(i)->paths)
                {
                    // Draw only paths that are not completely outside the viewing area
                    if (m_imageView.intersects(path.boundingRect()))
                        painter.drawPath(path);
                }
            }
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

            const segvdef *seg = ::controller.getChip().getSegment(net);
            for (const auto &path : seg->paths)
                painter.drawPath(path);
        }
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Draw latches
    //------------------------------------------------------------------------
    if (m_drawLatches)
    {
        painter.save();
        ::controller.getChip().drawLatches(painter, m_imageView.toAlignedRect());
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Draw transistors
    //------------------------------------------------------------------------
    if ((m_drawActiveTransistors || m_drawAllTransistors) && mouseOff)
    {
        painter.save();
        ::controller.getChip().expDrawTransistors(painter, m_imageView.toAlignedRect(), m_drawAllTransistors);
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Highlight two features on top of the image: a box (a transistor) and
    // a segment (a signal), both of which were selected via the "Find" dialog
    //------------------------------------------------------------------------
    {
        painter.save();
        qreal guideLineScale = 1.0 / m_scale + 1;
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
        if (m_highlight_trans)
        {
            painter.drawRect(*m_highlight_trans);
            painter.setPen(QPen(Qt::white, guideLineScale, Qt::SolidLine));
            painter.drawLine(QPoint(0,0), m_highlight_trans->topLeft());
            painter.setPen(QPen(QColor(), 0, Qt::NoPen)); // No outlines
        }
        if (m_highlight_segment)
        {
            for (const auto &path : m_highlight_segment->paths)
                painter.drawPath(path);
            painter.setPen(QPen(Qt::white, guideLineScale, Qt::SolidLine));
            painter.drawLine(QPoint(0,0), m_highlight_segment[0].paths[0].elementAt(0));
        }
        painter.restore();
    }
    //------------------------------------------------------------------------
    // Dynamically write nearby net names (experimental)
    //------------------------------------------------------------------------
    if (m_drawNetNames)
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
        ::controller.getAnnotation().draw(painter, m_imageView.toAlignedRect(), m_scale);
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
    if (ms > 250) // Show the drawing perf impact if it takes more than this many milliseconds
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
    if (event->type() == QEvent::ToolTip) // XXX Is this now redundant? We already use tooltip with Ctrl key
    {
        QPoint pos = m_invtx.map(m_mousePos);
        QVector<net_t> nets = ::controller.getChip().getNetsAt<false>(pos.x(), pos.y());
        if (nets.count() == 1)
        {
            QStringList tooltip { ::controller.getNetlist().get(nets[0]), ::controller.getTip().get(nets[0]) };
            tooltip.removeAll({}); // Remove empty components (from the above, if none defined)
            QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
            QToolTip::showText(helpEvent->globalPos(), tooltip.join("<br>"));

            event->ignore();
            return true;
        }
        QToolTip::hideText();
        event->ignore();
    }

    // Handle image scaling by multi-point touch
    switch (event->type())
    {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        {
            QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
            QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
            if (touchPoints.count() == 2)
            {
                // Determine the relative scale factor
                const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
                const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();
                qreal touchScale = QLineF(touchPoint0.pos(), touchPoint1.pos()).length() /
                                   QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();
                if (touchEvent->touchPointStates() & Qt::TouchPointPressed)
                    m_touchScale =  m_scale / touchScale; // On "pressed" event, set the scale factor
                m_scale = touchScale * m_touchScale;
                setZoom(m_scale);
            }
            return true;
        }
        default: break;
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

        moveBy(QPointF(dX/m_scale, dY/m_scale));
    }
    else if (m_mouseRightPressed) // With a right mouse button pressed: define the selection area
    {
        QPoint pos1 = m_invtx.map(m_pinMousePos);
        QPoint pos2 = m_invtx.map(event->pos());
        // If a shift key has been pressed, align the mouse coordinates to a grid in the texture space
        bool shift = QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
        if (shift)
        {
            pos2.setX(((pos2.x() + 7) & ~7) - 1); // "Greedy" selection
            pos2.setY(((pos2.y() + 7) & ~7) - 1);
        }
        m_areaRect.setTopLeft(pos1);
        m_areaRect.setBottomRight(pos2);

        QString coords = QString::asprintf("%d,%d %+d%+d", pos1.x(), pos1.y(), m_areaRect.width(), m_areaRect.height());
        m_ov->setCoords(coords);

        update();
    }
    else
    {
        // With no buttons pushed, update information on which nets or objects the mouse is pointing to
        QPoint imageCoords = m_invtx.map(event->pos());
        if (m_image.valid(imageCoords.x(), imageCoords.y()))
        {
            m_ov->setCoords(QString("%1,%2").arg(imageCoords.x()).arg(imageCoords.y()));

            const QVector<net_t> nets = ::controller.getChip().getNetsAt<true>(imageCoords.x(), imageCoords.y());
            QStringList netNames = ::controller.getNetlist().get(nets); // Translate net numbers to names
            const tran_t trans = ::controller.getChip().getTransistorAt(imageCoords.x(), imageCoords.y());
            if (trans)
                netNames.insert(0, QString("t%1").arg(trans)); // Insert the transistor name at the front
            netNames.removeAll(QString()); // Remove any blanks (likely due to an infrequent transistor area)
            netNames.removeDuplicates(); // Don't you simply love Qt?
            m_ov->setInfoLine(1, netNames.join(", "));

            QString tip = nets.count() ? ::controller.getTip().get(nets[0]) : QString();
            m_ov->setInfoLine(2, trans ? ::controller.getNetlist().transInfo(trans) : tip);
        }
        else
        {
            // Oops - the pointer is in this widget, but it's not currently over
            // the image, so we have no data to report.
            m_ov->clearInfoLine(0);
            m_ov->setCoords(QString());
        }
    }
}

void WidgetImageView::mousePressEvent(QMouseEvent *event)
{
    m_pinMousePos = event->pos();
    // If a shift key has been pressed, align the mouse coordinates to a grid in the texture space
    bool shift = QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    if (shift)
    {
        // First, we have to map it to texture coordinates so we can snap it to a grid, and then back to screen space
        QPoint pos = m_invtx.map(m_pinMousePos);
        pos.setX(pos.x() & ~7); // Initial pixel will always be included
        pos.setY(pos.y() & ~7);
        m_pinMousePos = m_tx.map(pos);
    }
    m_mouseLeftPressed = event->button() == Qt::LeftButton;
    m_mouseRightPressed = event->button() == Qt::RightButton;
    m_drawSelection = m_mouseRightPressed;
}

void WidgetImageView::mouseReleaseEvent(QMouseEvent *event)
{
    m_mouseLeftPressed = false;
    m_mouseRightPressed = false;
    m_drawSelection = event->button() == Qt::RightButton;
    // Releasing the right mouse button opens a custom context menu
    if (event->button() == Qt::RightButton)
    {
        if (!m_areaRect.isNull()) // Only display selected area when width, height > 0
        {
            QPoint pos1 = m_invtx.map(m_pinMousePos);
            qInfo() << "Selected area:" << QString("%1,%2,%3,%4")
                       .arg(pos1.x()).arg(pos1.y()).arg(m_areaRect.width()).arg(m_areaRect.height());
        }
        contextMenu(event->pos());
    }
    update();
}

/*
 * Double-clicking the mouse on a point in the image selects a net to trace
 * Selecting a point with no valid nets clears the selection
 */
void WidgetImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_pinMousePos = QPoint();
    m_areaRect.setRect(0,0,0,0);

    if (event->button() == Qt::LeftButton)
    {
        m_mousePos = event->pos();
        QPoint imageCoords = m_invtx.map(event->pos());
        if (m_image.valid(imageCoords.x(), imageCoords.y()))
        {
            // Select only one valid net number that is not vss or vcc
            QVector<net_t> nets = ::controller.getChip().getNetsAt<false>(imageCoords.x(), imageCoords.y());
            m_drivingNets.clear();
            if (nets.count() >= 1) // At least one valid net, set it as the base selected net
            {
                m_drivingNets.append(nets[0]);
                QString s = ::controller.getNetlist().netInfo(nets[0]);
                qInfo() << s.replace('\n', ' ');
            }
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
    setZoom(m_scale);
}

void WidgetImageView::leaveEvent(QEvent *)
{
    m_ov->clearInfoLine(0);
}

void WidgetImageView::keyPressEvent(QKeyEvent *event)
{
    // Approximate image move offsets in the texture space
    qreal dx = qreal(m_image.width()) / (m_viewPort.width() * m_scale * 100);
    qreal dy = qreal(m_image.height()) / (m_viewPort.height() * m_scale * 200);
    int i = -1;
    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9)
        i = event->key() - Qt::Key_1;
    else if (event->key() >= Qt::Key_A && event->key() <= Qt::Key_K)
        i = event->key() - Qt::Key_A + 9;
    else
    switch (event->key())
    {
    case Qt::Key_Escape: // ESC key removes, in this order: Found transistor and net; driven by; selected net
        if (m_highlight_segment || m_highlight_trans)
        {
            m_highlight_trans = nullptr;
            m_highlight_segment = nullptr;
        }
        else if (m_drivingNets.count() > 1)
            m_drivingNets.remove(1, m_drivingNets.count() - 1);
        else if (m_drivingNets.count() == 1)
            m_drivingNets.clear();
        break;
    case Qt::Key_F1:
        switch(m_view_mode)
        {
            case Fit: setZoomMode(Fill); break;
            case Fill: setZoomMode(Identity); break;
            case Identity: setZoomMode(Fit); break;
            case Value: setZoomMode(Fit); break;
        }
        break;
    case Qt::Key_X:
        m_drawActiveNets = !m_drawActiveNets;
        m_ov->setButton(0, m_drawActiveNets);
        break;
    case Qt::Key_Space:
        m_drawAnnotations = !m_drawAnnotations;
        m_ov->setButton(1, m_drawAnnotations);
        break;
    case Qt::Key_T:
        m_drawActiveTransistors = !m_drawActiveTransistors;
        m_drawAllTransistors = false;
        m_ov->setButton(2, m_drawActiveTransistors);
        break;
    case Qt::Key_Period:
        m_drawAllTransistors = !m_drawAllTransistors; break;
    case Qt::Key_L:
        m_drawLatches = !m_drawLatches;
        m_ov->setButton(3, m_drawLatches);
        break;
    case Qt::Key_N: m_drawNetNames = !m_drawNetNames; break;
    case Qt::Key_Left: moveBy(QPointF(dx,0)); break;
    case Qt::Key_Right: moveBy(QPointF(-dx,0)); break;
    case Qt::Key_Up: moveBy(QPointF(0,dy)); break;
    case Qt::Key_Down: moveBy(QPointF(0,-dy)); break;
    case Qt::Key_PageUp: setZoom(m_scale * 1.2); break;
    case Qt::Key_PageDown: setZoom(m_scale / 1.2); break;
    }

    setImage(i);
}

void WidgetImageView::setImage(int i, bool forceCtrl)
{
    if (i >= 0) // called from keyPressEvent() might not be selecting an image
    {
        bool ctrl = m_enable_ctrl && QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        if (forceCtrl || ctrl) // Compositing multiple images
        {
            QImage &image = ::controller.getChip().getImage(i);
            QPainter painter(&m_image);
            painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
            painter.drawImage(0,0, image);
            painter.end();
            m_ov->selectImage(image.text("name"), true);
        }
        else // Simple image view
        {
            m_image = ::controller.getChip().getImage(i); // Creates a shallow image copy
            m_ov->selectImage(m_image.text("name"), false);
        }
    }
    update();
}

/*
 * Context menu handler, called when the user right-clicks somewhere on the image view
 */
void WidgetImageView::contextMenu(const QPoint& pos)
{
    // If the user dragged the selection rectangle in the "opposite" way, we need to swap the corners
    m_areaRect = m_areaRect.normalized();

    // If the selection area is too small, clear it
    if ((m_areaRect.width() < 4) || (m_areaRect.height() < 4))
        m_areaRect.setRect(0,0,0,0);

    QMenu contextMenu(this);

    // "Add annotation" option, only if the selection area has some width to it
    QAction actionAddAnnotation("Add annotation...", this);
    connect(&actionAddAnnotation, SIGNAL(triggered()), this, SLOT(addAnnotation()));
    if (m_drawSelection && m_areaRect.width())
        contextMenu.addAction(&actionAddAnnotation);

    // "Driving nets" option, only if the user selected a node but not selection area (less confusing)
    QAction actionDriving("Driving nets", this);
    connect(&actionDriving, SIGNAL(triggered()), this, SLOT(netsDriving()));
    if ((m_drivingNets.count() >= 1) && !m_areaRect.width())
        contextMenu.addAction(&actionDriving);

    // "Driven by" option, only if the user selected a node but not selection area (less confusing)
    QAction actionDriven("Driven by", this);
    connect(&actionDriven, SIGNAL(triggered()), this, SLOT(netsDriven()));
    if ((m_drivingNets.count() >= 1) && !m_areaRect.width())
        contextMenu.addAction(&actionDriven);

    // "Edit tip..." option, only if the user picked a single net node but not selection area (less confusing)
    QAction actionEditTip("Edit tip...", this);
    connect(&actionEditTip, SIGNAL(triggered()), this, SLOT(editTip()));
    if ((m_drivingNets.count() == 1) && !m_areaRect.width())
        contextMenu.addAction(&actionEditTip);

    // "Edit net name..." option, only if the user picked a single net node but not selection area (less confusing)
    QAction actionEditNetName("Edit net name...", this);
    connect(&actionEditNetName, SIGNAL(triggered()), this, SLOT(editNetName()));
    if ((m_drivingNets.count() == 1) && !m_areaRect.width())
        contextMenu.addAction(&actionEditNetName);

    QAction actionEditAnnotation("Edit annotations...", this);
    connect(&actionEditAnnotation, SIGNAL(triggered()), this, SLOT(editAnnotations()));
    contextMenu.addAction(&actionEditAnnotation);

    QAction actionSyncViews("Sync image views", this);
    connect(&actionSyncViews, &QAction::triggered, this, [=]() { emit ::controller.syncView(m_tex, m_scale); });
    contextMenu.addAction(&actionSyncViews);

    // "Schematic" option, only if the user selected a node but not selection area (less confusing)
    QAction actionSchematic("Schematic...", this);
    connect(&actionSchematic, SIGNAL(triggered()), this, SLOT(viewSchematic()));
    if ((m_drivingNets.count() >= 1) && !m_areaRect.width())
    {
        // In addition, you can only ask for schematic of a net that is actually driving some gates
        // XXX move this check somewhere else
        if (::controller.getNetlist().netsDriving(m_drivingNets[0]).count())
            contextMenu.addAction(&actionSchematic);
    }

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
    QString text = QInputDialog::getText(this, "Add annotation", "Annotation text:",
                                         QLineEdit::Normal, "", &ok, Qt::MSWindowsFixedSizeDialogHint);
    if (ok && text.trimmed().length() > 0)
    {
        ::controller.getAnnotation().add(text, m_areaRect);
        DialogEditAnnotations dlg(this);
        QVector<uint> sel { ::controller.getAnnotation().count() - 1 };
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
        sel = ::controller.getAnnotation().get(pos);
    else
        sel = ::controller.getAnnotation().get(m_areaRect);
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
    QString oldTip = ::controller.getTip().get(net);
    bool ok;
    QString tip = QInputDialog::getText(this, "Edit tip", QString("Enter the tip for the selected net %1 (%2)").arg(name,QString::number(net)),
                                        QLineEdit::Normal, oldTip, &ok, Qt::MSWindowsFixedSizeDialogHint);
    if (ok)
        ::controller.getTip().set(tip, net);
}

/*
 * Appends nets that the selected net is driving
 */
void WidgetImageView::netsDriving()
{
    Q_ASSERT(m_drivingNets.count() >= 1);
    m_drivingNets.remove(1, m_drivingNets.count() - 1); // Leave only the primary selected node
    QVector<net_t> driving = ::controller.getNetlist().netsDriving(m_drivingNets[0]);
    m_drivingNets.append(driving);
    QStringList list = ::controller.getNetlist().get(driving);
    QString name = ::controller.getNetlist().get(m_drivingNets[0]);
    if (name.isEmpty())
        name = QString::number(m_drivingNets[0]);
    qInfo() << "Net" << name << "driving" << list.count() << "nets" << list;
}

/*
 * Appends nets that drive the selected net
 */
void WidgetImageView::netsDriven()
{
    Q_ASSERT(m_drivingNets.count() >= 1);
    m_drivingNets.remove(1, m_drivingNets.count() - 1); // Leave only the primary selected node
    QVector<net_t> driven = ::controller.getNetlist().netsDriven(m_drivingNets[0]);
    m_drivingNets.append(driven);
    QStringList list = ::controller.getNetlist().get(driven);
    QString name = ::controller.getNetlist().get(m_drivingNets[0]);
    if (name.isEmpty())
        name = QString::number(m_drivingNets[0]);
    qInfo() << "Net" << name << "driven by" << list.count() << "nets" << list;
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
    QString newName = QInputDialog::getText(this, "Edit net name", "Enter the name (alias) of the selected net " + QString::number(newNet) + "\n",
                                            QLineEdit::Normal, oldName, &ok, Qt::MSWindowsFixedSizeDialogHint);
    newName = newName.trimmed().toLower(); // Trim spaces and keep net names lowercased
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
 * Creates a new Schematic window showing the primary selected net's logic diagram
 */
void WidgetImageView::viewSchematic()
{
    Q_ASSERT(m_drivingNets.count() >= 1);
    net_t net = m_drivingNets[0];

    // Calculate the logic equation for the net
    Logic *lr = ::controller.getNetlist().getLogicTree(net);
    qInfo() << Logic::flatten(lr);

    // If the user pressed Ctrl key, we will *not* optimize logic tree network
    bool ctrl = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
    if (!ctrl)
        ::controller.getNetlist().optimizeLogicTree(&lr);

    DialogSchematic *sch = new DialogSchematic(this, lr);
    connect(sch, SIGNAL(doShow(QString)), this, SLOT(onFind(QString)));
    connect(sch, &DialogSchematic::doNewSchematic, this, [=](net_t net) { m_drivingNets.prepend(net); viewSchematic(); } );
    if (ctrl) // Update schematic's window title if the net we passed to it was not optimized
        sch->setWindowTitle(sch->windowTitle() + " (not optimized)");
    sch->show();
}

/*
 * Search for the named feature
 * This function is called when the user enters some text in the Find box
 * Typing [Enter] in the empty Find dialog will re-trigger the highlight flashing
 */
void WidgetImageView::onFind(QString text)
{
    bool ok;
    const transvdef *trans = nullptr;
    if (text.length() == 0)
        m_timer_tick = 10; // With no text, restart the highlights flashing
    else
    {
        m_timer_tick = 0; // Stop the highlights flashing

        // Search the transistors first
        if (text.startsWith('t'))
        {
            tran_t transId = text.mid(1).toUInt(&ok);
            if (ok)
                trans = ::controller.getChip().getTrans(transId);
        }
        if (trans)
        {
            m_highlight_trans = &trans->box;
            qInfo() << "Found transistor" << text;
            m_timer_tick = 10;
        }
        else // Search the nets, next...
        {
            // Search nets by number and then by name
            net_t netnum = text.toUInt(&ok);
            if (!ok) // Check if the input is a net name
            {
                netnum = ::controller.getNetlist().get(text);
                ok = netnum > 0;
            }
            if (ok)
            {
                const segvdef *seg = ::controller.getChip().getSegment(netnum);
                if (seg->netnum)
                {
                    m_highlight_segment = seg;
                    qInfo() << "Found net" << netnum << text;
                    m_timer_tick = 10;
                }
                else
                    qInfo() << text << "not found!";
            }
            else
                qInfo() << text << "not found!";
        }
    }
    update();
}

/*
 * Prints the img view zoom and position state as JavaScript commands which can be later used to restore it
 */
void WidgetImageView::state()
{
    QString s = QString::asprintf("img.setZoom(%.3f); img.setPos(%d,%d)", m_scale, int(m_tex.x() * m_image.width()), int(m_tex.y() * m_image.height()));
    qInfo() << s;
}

/*
 * Supporting drag-and-drop of json files
 */
void WidgetImageView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.count() != 1)
            return;
        QFileInfo fi(urls.first().toLocalFile());
        if (fi.suffix().toLower() != "json")
            return;
        m_dropppedFile = fi.absoluteFilePath();
        qDebug() << m_dropppedFile;
        event->setDropAction(Qt::LinkAction);
        event->accept();
    }
}

void WidgetImageView::dropEvent(QDropEvent *)
{
    ::controller.getAnnotation().load(m_dropppedFile);
    update();
}
