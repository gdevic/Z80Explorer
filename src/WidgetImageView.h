#ifndef WIDGETIMAGEVIEW_H
#define WIDGETIMAGEVIEW_H

#include <QWidget>

class WidgetImageOverlay;
struct segdef;

namespace Ui { class WidgetImageView; }

/*
 * This class implement an image viewer tied to the set of images stored in
 * ClassChip. It allows the user to move and zoom into images.
 */
class WidgetImageView : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetImageView(QWidget *parent = nullptr);
    ~WidgetImageView();

    void init();
    const QImage& getImage();           // Returns the current QImage; is never NULL
    QRectF getImageView();              // Return the normalized viewport in the image space
    enum ZoomType { Fit, Fill, Identity, Value }; // List of possible zoom modes
    Q_ENUM(ZoomType);                   // Register enum names inside a QMetaObject

signals:
    void imageMoved(QPointF);           // Image is moved by this control to new coordinates
    void imageZoomed(int);              // Image is zoomed by specified number of steps (+/-)

    // Send the XY coordinates of the pointer (in image coordinates)
    // and the color of the pixel under the pointer
    void pointerData(int x, int y, uint8_t r, uint8_t g, uint8_t b);

    // Indicate that the pointer is not currently over the image
    void clearPointerData();

    // contextMenuRequestedAt() is sent when the user presses
    // the right mouse button over the widget. The signal includes
    // a reference to this widget, and the global position of the
    // click.
    void contextMenuRequestedAt(WidgetImageView* widget, const QPoint& where);

public slots:
    void onRefresh();                   // Called when class chip changes image
    void setImage(const QImage &);      // Makes a copy of the image and sets it as current
    void setZoomMode(ZoomType);         // Set the view mode
    void setZoom(double);               // Set the zoom value
    void moveBy(QPointF);               // Moves the image in the pane by specified normalized delta
    void moveTo(QPointF);               // Moved the image in the pane to normalized coordinate
    void imageCenterH();                // Centers the image horizontally
    void imageCenterV();                // Centers the image vertically
    void onCoords();                    // Open coordinate dialog and center image on user input coordinates

private slots:
    void onFind(QString text);          // Search for the named feature
    void onTimeout();                   // Timer timeout handler
    void onRunStopped(uint);            // Called by the sim when the current run stops at a given half-cycle

private:
    Ui::WidgetImageView *ui;

    QImage  m_image;                    // Current image
    QSize   m_panelSize;                // View panel size, drawable area
    QPointF m_tex;                      // Texture coordinate to map to view center (normalized)
    qreal   m_scale;                    // Scaling value
    ZoomType m_view_mode;               // Current zoom mode
    bool    m_drawActiveNets { true };  // Draw active nets

    QPoint  m_mousePos;                 // Current mouse position
    QPoint  m_pinMousePos;              // Mouse position at the time of button press
    bool    m_mousePressed;             // Mouse button is pressed

    QTransform m_tx;                    // Transformation matrix from normalized image to screen space
    QTransform m_invtx;                 // Transformation matrix from screen to normalized image space
    QRect      m_viewPort;              // Bounding rectangle of the current screen view
    QRectF     m_imageView;             // Helper variable for getImageView()
    WidgetImageOverlay *m_ov;           // Image overlay class
    QTimer     *m_timer;                // Image refresh timer
    uint       m_timer_tick;            // Timer timeout tick counter
    const segdef *m_highlight_segment;  // Segment to highlight in the current image
    const QRect *m_highlight_box;       // Box to highlight in the current image

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

#endif // WIDGETIMAGEVIEW_H
