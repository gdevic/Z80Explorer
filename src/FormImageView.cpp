/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <QPainter>
#include <QRgb>
#include <QSpacerItem>
#include <QtGlobal>

#include "FormImageView.h"
#include "ui_FormImageView.h"

//============================================================================
// Class constructor and destructor
//============================================================================

FormImageView::FormImageView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormImageView),
    m_mousePressed(false),
    m_gridLayout(0)
{
    ui->setupUi(this);
    setViewMode(Fit);
    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect the view's internal intent to move its image (for example,
    // when the user drags it with a mouse)
    connect(this, SIGNAL(imageMoved(QPointF)), this, SLOT(moveBy(QPointF)));

    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(contextMenuRequested(const QPoint&)));
}

FormImageView::~FormImageView()
{
    delete ui;
}

//============================================================================
// Public Methods
//============================================================================

void FormImageView::setImage(const QImage &img)
{
    m_image = img;
    // When the image changes, refresh the transform.
    // This helps the Navigator get initial sizing correct.
    calcTransform();
    update();
}

const QImage& FormImageView::getImage()
{
    return m_image;
}

void FormImageView::setViewMode(ZoomType mode)
{
    qreal sx = (qreal) width()/m_image.width();
    qreal sy = (qreal) height()/m_image.height();

    switch(mode)
    {
    case Fit:                       // Fit
        m_tex = QPointF(0.5, 0.5);   // Map texture center to view center
        m_scale = sx > sy ? sy : sx;
        if(sx>1.0 && sy>1.0)
            m_scale = 1.0;
        break;

    case Fill:                      // Fill
        m_tex = QPointF(0.5, 0.5);   // Map texture center to view center
        m_scale = sx > sy ? sx : sy;
        if(sx>1.0 && sy>1.0)
            m_scale = 1.0;
        break;

    case Identity:                  // 1:1 Zoom ratio
        m_scale = 1.0;
        break;

    default:                        // Any other value is a free zoom ratio
        break;
    }

    calcTransform();
    update();
}

void FormImageView::setZoom(double value)
{
    // Check that the zoom value is valid
    m_scale = qBound(0.1, value, 10.0);
    setViewMode(Value);
}

void FormImageView::moveBy(QPointF delta)
{
    m_tex -= delta;
    clampImageCoords(m_tex);
    update();
}

void FormImageView::moveTo(QPointF pos)
{
    m_tex = pos;
    clampImageCoords(m_tex);
    update();
}

void FormImageView::imageCenterH()
{
    m_tex.setX(0.5);
    update();
}

void FormImageView::imageCenterV()
{
    m_tex.setY(0.5);
    update();
}

//============================================================================
// Callbacks
//============================================================================

// Clamp the image coordinates into the range [0,1]
void FormImageView::clampImageCoords(QPointF &tex)
{
    if(tex.x()<0.0) tex.setX(0.0);
    if(tex.x()>1.0) tex.setX(1.0);

    if(tex.y()<0.0) tex.setY(0.0);
    if(tex.y()>1.0) tex.setY(1.0);
}

void FormImageView::getJoinedSizeHint(QSizeF &size)
{
    double sx = (double) m_panelSize.width() / 4;
    double ix = m_image.width() * m_scale / 2;
    double fx = sx/ix;
    size.setWidth(fx);

    double sy = (double) m_panelSize.height() / 4;
    double iy = m_image.height() * m_scale / 2;
    double fy = sy/iy;
    size.setHeight(fy);
}

// Return the coordinates within the image that the view is clipped at, given
// its position and zoom ratio. This rectangle corresponds to what the user sees.
QRectF FormImageView::getImageView()
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

    m_imageView = QRectF(t0, t1);

    return m_imageView;
}

//============================================================================
// Callbacks
//============================================================================

void FormImageView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    m_viewPort = painter.viewport();

    // Transformation allows us to simply draw as if the source and target are the same size
    calcTransform();
    QRect size(0, 0, m_image.width(), m_image.height());

    painter.setTransform(m_tx);
    painter.drawImage(size, m_image, size);
}

void FormImageView::calcTransform()
{
    int sx = m_image.width();
    int sy = m_image.height();

    QTransform mtr1(1, 0, 0, 1, -sx * m_tex.x(), -sy * m_tex.y());
    QTransform msc1(m_scale, 0, 0, m_scale, 0, 0);
    QTransform mtr2(1, 0, 0, 1, m_viewPort.width()/2, m_viewPort.height()/2);

    m_tx = mtr1 * msc1 * mtr2;
    m_invtx = m_tx.inverted();
}

void FormImageView::resizeEvent(QResizeEvent * event)
{
    m_panelSize = event->size();
}

//============================================================================
// Mouse tracking
//============================================================================

void FormImageView::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();

    if(m_mousePressed)
    {
        // This is beautiful.
        double dX = m_mousePos.x() - m_pinMousePos.x();
        double dY = m_mousePos.y() - m_pinMousePos.y();
        m_pinMousePos = m_mousePos;

        dX = dX / m_image.width();
        dY = dY / m_image.height();

        emit imageMoved(QPointF(dX/m_scale, dY/m_scale));
    }
    else
    {
        // With no buttons pushed, a mouse move needs to update the XY/RGB
        // information in the application status bar. So let's emit the signal
        // that does that.
        QPoint imageCoords = m_invtx.map(event->pos());
        if (m_image.valid(imageCoords.x(), imageCoords.y()))
        {
            QRgb imageColor = m_image.pixel(imageCoords);
            emit pointerData(imageCoords.x(), imageCoords.y(), qRed(imageColor), qGreen(imageColor), qBlue(imageColor));
        }
        else
        {
            // Oops - the pointer is in this widget, but it's not currently over
            // the image, so we have no data to report.
            emit clearPointerData();
        }
    }
}

void FormImageView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        return; // handled via customContextMenuRequested() signal

    setCursor(QCursor(Qt::ClosedHandCursor));
    m_pinMousePos = event->pos();
    m_mousePressed = true;

    emit gotFocus(m_viewId);
}

void FormImageView::mouseReleaseEvent (QMouseEvent *)
{
    setCursor(QCursor(Qt::OpenHandCursor));
    m_mousePressed = false;
}

void FormImageView::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0)
        m_scale = m_scale * 1.2;
    else
        m_scale = m_scale / 1.2;
    emit setZoom(m_scale);
}

void FormImageView::leaveEvent(QEvent* event)
{
    emit clearPointerData();
}

// HUD support
void FormImageView::createLayout()
{
    if (m_gridLayout)
        return;

    QGridLayout* gl = m_gridLayout = new QGridLayout(this);
    // This grid is 5x5. The odd columns and rows will
    // always contain a spacer item while the even items can
    // contain UI widgets. Setting the stretch values like this
    // allows the UI widgets to assume a natural size, while the
    // slack is taken up by the spacers in between them.
    for (int r=0; r<5; r++)
    {
        gl->setRowStretch(r, r%2 ? 1 : 0);  // odd stretch = 1, even stretch = 0
        gl->setColumnStretch(r, r%2 ? 1 : 0);

        for (int c=0; c<5; c++)
        {
            gl->addItem(new QSpacerItem(1,1), r, c);
        }
    }
}

void FormImageView::setHudWidget(HudWidgetPos pos, QWidget *w)
{
    if (!w)
        return;

    createLayout();

    w->setParent(this);
    w->show();

    int r = 0, c = 0;
    switch (pos)
    {
        case TopLeft:
            r = 0; c = 0;
            break;
        case TopCenter:
            r = 0; c = 2;
            break;
        case TopRight:
            r = 0; c = 4;
            break;
        case MidLeft:
            r = 2; c = 0;
            break;
        case Center:
            r = 2; c = 2;
            break;
        case MidRight:
            r = 2; c = 4;
            break;
        case BottomLeft:
            r = 4; c = 0;
            break;
        case BottomCenter:
            r = 4; c = 2;
            break;
        case BottomRight:
            r = 4; c = 4;
            break;
    }
    m_gridLayout->addWidget(w, r, c);
}

void FormImageView::contextMenuRequested(const QPoint& localWhere)
{
    emit contextMenuRequestedAt(this, mapToGlobal(localWhere));
}
