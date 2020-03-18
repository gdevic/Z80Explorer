#include "FormImageView.h"
#include "ui_FormImageView.h"
#include "ClassChip.h"
#include "FormImageOverlay.h"

#include <QDebug>
#include <QGridLayout>
#include <QPainter>
#include <QResizeEvent>
#include <QRgb>
#include <QSpacerItem>
#include <QtGlobal>

//============================================================================
// Class constructor and destructor
//============================================================================

FormImageView::FormImageView(QWidget *parent, ClassChip *chip) :
    QWidget(parent),
    ui(new Ui::FormImageView),
    m_chip(chip),
    m_mousePressed(false)
{
    ui->setupUi(this);
    setZoomMode(Fit);
    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(QCursor(Qt::CrossCursor));

    // Connect the view's internal intent to move its image (for example,
    // when the user drags it with a mouse)
    connect(this, SIGNAL(imageMoved(QPointF)), this, SLOT(moveBy(QPointF)));

    connect(m_chip, SIGNAL(refresh()), this, SLOT(onRefresh()));

    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(contextMenuRequested(const QPoint&)));

    // Initial set image
    setImage(m_chip->getImage(0));

    // Create and set the image overlay widget
    m_ov = new FormImageOverlay(this);
    m_ov->setParent(this);
    m_ov->move(10, 10);
    m_ov->show();
    connect(this, SIGNAL(pointerData(int,int,uint8_t,uint8_t,uint8_t)), m_ov, SLOT(onPointerData(int,int,uint8_t,uint8_t,uint8_t)));
    connect(this, SIGNAL(clearPointerData()), m_ov, SLOT(onClearPointerData()));

    connect(m_ov, SIGNAL(actionBuild()), m_chip, SLOT(onBuild()));
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
    setZoom(0.1);
    // When the image changes, refresh the transform.
    // This helps the Navigator get initial sizing correct.
    calcTransform();
    update();
}

const QImage& FormImageView::getImage()
{
    return m_image;
}

void FormImageView::setZoomMode(ZoomType mode)
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
}

void FormImageView::setZoom(double value)
{
    // Make sure that the zoom value is in the sane range
    m_scale = qBound(0.1, value, 10.0);
    setZoomMode(Value);
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

// Called when class chip changes image
void FormImageView::onRefresh()
{
    m_image = m_chip->getLastImage();
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

            QList<int> nodes = m_chip->getNodesAt(imageCoords.x(), imageCoords.y());
            QString s;
            for(int &i : nodes)
            {
                s.append(QString::number(i));
                s.append(',');
            }
            m_ov->setText(1, s);
            QList<QString> names = m_chip->getNodenamesFromNodes(nodes);
            m_ov->setText(2, names.join(','));
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

    m_pinMousePos = event->pos();
    m_mousePressed = true;
}

void FormImageView::mouseReleaseEvent (QMouseEvent *)
{
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

void FormImageView::leaveEvent(QEvent *)
{
    emit clearPointerData();
}

void FormImageView::keyPressEvent(QKeyEvent *event)
{
    //enum ChipLayer { Burried, Diffusion, Ions, Metal, Pads, Poly, Vias };
    bool alt = event->modifiers() & Qt::AltModifier;
    if (event->key() >= '1' && event->key() <= '9')
    {
        uint i = event->key() - '0' + (alt ? 9 : 0); // assuming 9 basic Z80 chip resource images
        m_image = m_chip->getImage(i);
        update();
    }
    if (event->key() == Qt::Key_F)
    {
        switch(m_view_mode)
        {
            case Fit: setZoomMode(Fill); break;
            case Fill: setZoomMode(Identity); break;
            case Identity: setZoomMode(Fit); break;
            case Value: setZoomMode(Fit); break;
        }
    }
}

void FormImageView::contextMenuRequested(const QPoint& localWhere)
{
    emit contextMenuRequestedAt(this, mapToGlobal(localWhere));
}
