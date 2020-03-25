#include "FormImageView.h"
#include "ui_FormImageView.h"
#include "ClassChip.h"
#include "FormImageOverlay.h"

#include <QDebug>
#include <QGridLayout>
#include <QInputDialog>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QRgb>
#include <QSpacerItem>
#include <QtGlobal>
#include <QTimer>

//============================================================================
// Class constructor and destructor
//============================================================================

FormImageView::FormImageView(QWidget *parent, ClassChip *chip) :
    QWidget(parent),
    ui(new Ui::FormImageView),
    m_chip(chip),
    m_image(QImage()),
    m_view_mode(Fill),
    m_mousePressed(false),
    m_highlight_path(nullptr),
    m_highlight_box(nullptr)
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

    // Create and set the image overlay widget
    m_ov = new FormImageOverlay(this);
    m_ov->setParent(this);
    m_ov->move(10, 10);
    m_ov->show();

    // Create a timer that updates image every 1/2 seconds to show highlight blink
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    m_timer->start();

    connect(this, SIGNAL(pointerData(int,int,uint8_t,uint8_t,uint8_t)), m_ov, SLOT(onPointerData(int,int,uint8_t,uint8_t,uint8_t)));
    connect(this, SIGNAL(clearPointerData()), m_ov, SLOT(onClearPointerData()));
    connect(m_ov, SIGNAL(actionBuild()), m_chip, SLOT(onBuild()));
    connect(m_ov, SIGNAL(actionCoords()), this, SLOT(onCoords()));
    connect(m_ov, SIGNAL(actionFind(QString)), this, SLOT(onFind(QString)));
}

FormImageView::~FormImageView()
{
    delete ui;
}

/*
 * Timer timeout handler
 */
void FormImageView::onTimeout()
{
    if (m_timer_tick)
        m_timer_tick--;
    update();
}

//============================================================================
// Public Methods
//============================================================================

void FormImageView::setImage(const QImage &img)
{
    m_image = img;
    m_ov->setText(3, m_image.text("name"));
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
    setFocus();
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

/*
 * Open coordinate dialog and center image on user input coordinates
 */
void FormImageView::onCoords()
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

// Called when class chip changes image
void FormImageView::onRefresh()
{
    bool is_init = m_image.isNull(); // The very first image after init
    m_image = m_chip->getLastImage();
    m_ov->setText(3, m_image.text("name"));
    update();
    if (is_init)
    {
        m_ov->setLayerNames(m_chip->getLayerNames());
        setZoomMode(Fit);
    }
}

// Clamp the image coordinates into the range [0,1]
void FormImageView::clampImageCoords(QPointF &tex)
{
    tex.setX(qBound(0.0, tex.x(), 1.0));
    tex.setY(qBound(0.0, tex.y(), 1.0));
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
    painter.translate(-0.5, -0.5); // Adjust for Qt's very precise rendering
    painter.drawImage(size, m_image, size);

    //------------------------------------------------------------------------
    // Draw two selected features on top of the image: box (a transistor) and
    // a path (a signal), both of which are selected with the "Find" dialog
    //------------------------------------------------------------------------
    if (m_timer_tick & 1)
    {
        painter.setBrush(QColor(255,255,0));
        painter.setPen(QPen(QColor(255,0,255), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
    }
    else
    {
        painter.setBrush(QColor(255,255,255));
        painter.setPen(QPen(QColor(255,255,255), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
    }
    if (m_highlight_box)
        painter.drawRect(*m_highlight_box);
    if (m_highlight_path)
        painter.drawPath(*m_highlight_path);
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
            for (int &i : nodes)
                s.append(QString::number(i)).append(',');
            QStringList trans = m_chip->getTransistorsAt(imageCoords.x(), imageCoords.y());
            for (QString name : trans)
                s.append(name).append(',');
            m_ov->setText(1, s);

            QStringList names = m_chip->getNodenamesFromNodes(nodes);
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
    bool alt = event->modifiers() & Qt::AltModifier;
    int i = -1;
    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9)
        i = event->key() - Qt::Key_1;
    else if (event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z)
        i = event->key() - Qt::Key_A + 9;
    else if (event->key() == Qt::Key_F1)
    {
        switch(m_view_mode)
        {
            case Fit: setZoomMode(Fill); break;
            case Fill: setZoomMode(Identity); break;
            case Identity: setZoomMode(Fit); break;
            case Value: setZoomMode(Fit); break;
        }
    }
    if (i >= 0)
    {
        if (alt) // Compositing multiple images view
        {
            QPainter painter(&m_image);
            painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
            painter.drawImage(0,0, m_chip->getImage(i));
            painter.end();
        }
        else // Simple image view
        {
            m_image = m_chip->getImage(i); // Creates a shallow image copy
            m_ov->setText(3, m_image.text("name"));
        }
        update();
    }
}

void FormImageView::contextMenuRequested(const QPoint& localWhere)
{
    emit contextMenuRequestedAt(this, mapToGlobal(localWhere));
}

/*
 * Search for the named feature
 * This function is called when the user enters some text in the Find box
 * Typing [Enter] in the empty Find dialog will re-trigger the highlight flash
 * Typing "0" will clear all highlights
 */
void FormImageView::onFind(QString text)
{
    if (text.length() == 0)
    {
        m_timer_tick = 10;
        return;
    }
    if (text=='0')
    {
        m_highlight_box = nullptr;
        m_highlight_path = nullptr;
        qDebug() << "Highlights cleared";
        return;
    }
    bool ok;
    if (text.startsWith(QChar('t')))
    {
        const transdef *trans = m_chip->getTrans(text);
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
    else
    {
        uint nodenum = text.toUInt(&ok);
        if (ok)
        {
            const segdef *seg = m_chip->getSegment(nodenum);
            if (seg)
            {
                m_highlight_path = &seg->path;
                qDebug() << "Found segment" << text;
                qDebug() << "Pullup:" << seg->pullup << "Layer:" << seg->layer << "Points:" << seg->points.count() << "Path:" << seg->path.elementCount();
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
