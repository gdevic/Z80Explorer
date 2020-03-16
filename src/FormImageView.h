#ifndef FORMIMAGEVIEW_H
#define FORMIMAGEVIEW_H

#include <QWidget>
#include <QResizeEvent>
#include <QGridLayout>

class ClassChip;

namespace Ui {
    class FormImageView;
}

//
// This class implements a single pane within a dual pane FormCompositeView
// and should be used only by that class.
//
class FormImageView : public QWidget
{
    Q_OBJECT

public:
    explicit FormImageView(QWidget *parent, ClassChip *chip);
    ~FormImageView();

    int m_viewId;                       // ID of this particular pane (0 or 1)

    const QImage& getImage();           // Returns the current QImage; is never NULL
    void getJoinedSizeHint(QSizeF &);
    QRectF getImageView();              // Return the normalized viewport in the image space
    enum ZoomType { Fit, Fill, Identity, Value };   // List of possible zoom modes

    // HUD widget support:
    // Position specification for fadeable overlay widgets
    enum HudWidgetPos {TopLeft, TopCenter, TopRight,
                       MidLeft, Center, MidRight,
                       BottomLeft, BottomCenter, BottomRight};
    void setHudWidget(HudWidgetPos pos, QWidget *w);

signals:
    void gotFocus(int);                 // Sends the ID of this pane (0 or 1) when focus changes
    void imageMoved(QPointF);           // Image is moved by this control to new coordinates
    void imageZoomed(int);              // Image is zoomed by specified number of steps (+/-)

    /**
     * Send the XY coordinates of the pointer (in image coordinates)
     * and the color of the pixel under the pointer.
     */
    void pointerData(int x, int y, uint8_t r, uint8_t g, uint8_t b);

    /**
     * Indicate that the pointer is not currently over the image.
     */
    void clearPointerData();

    /**
     * contextMenuRequestedAt() is sent when the user presses
     * the right mouse button over the widget. The signal includes
     * a reference to this widget, and the global position of the
     * click.
     */
    void contextMenuRequestedAt(FormImageView* widget, const QPoint& where);

public slots:
    void setImage(const QImage &);      // Makes a copy of the image and sets it as current
    void setViewMode(ZoomType);         // Set the view mode
    void setZoom(double);               // Set the zoom value
    void moveBy(QPointF);               // Moves the image in the pane by specified normalized delta
    void moveTo(QPointF);               // Moved the image in the pane to normalized coordinate
    void imageCenterH();                // Centers the image horizontally
    void imageCenterV();                // Centers the image vertically

private:
    Ui::FormImageView *ui;
    ClassChip *m_chip;

    QImage  m_image;                    // Current image
    QSize   m_panelSize;                // View panel size, drawable area
    QPointF m_tex;                      // Texture coordinate to map to view center (normalized)
    qreal   m_scale;                    // Scaling value
    ZoomType m_view_mode;

    QPoint  m_mousePos;                 // Current mouse position
    QPoint  m_pinMousePos;              // Mouse position at the time of button press
    bool    m_mousePressed;             // Mouse button is pressed

    QTransform m_tx;                    // Transformation matrix from normalized image to screen space
    QTransform m_invtx;                 // Transformation matrix from screen to normalized image space
    QRect      m_viewPort;              // Bounding rectangle of the current screen view
    QRectF     m_imageView;             // Helper variable for getImageView()
    QGridLayout* m_gridLayout;          // If HUD widgets are used, this is the layout for them

    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void wheelEvent(QWheelEvent *);
    void leaveEvent(QEvent* event);
    void keyPressEvent(QKeyEvent *e);

    void calcTransform();
    void clampImageCoords(QPointF &);
    void createLayout();

private slots:
    void contextMenuRequested(const QPoint& localWhere);
};

#endif // FORMIMAGEVIEW_H