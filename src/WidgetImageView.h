#ifndef WIDGETIMAGEVIEW_H
#define WIDGETIMAGEVIEW_H

#include "AppTypes.h"
#include <QQueue>
#include <QTimer>
#include <QWidget>

class WidgetImageOverlay;
struct segvdef;

namespace Ui { class WidgetImageView; }

/*
 * This class implement an image viewer tied to the set of images stored in
 * ClassVisual. It allows the user to move and zoom into images.
 */
class WidgetImageView : public QWidget
{
    Q_OBJECT                            //* <- Methods of the scripting object "img" below

public:
    explicit WidgetImageView(QWidget *parent = nullptr);
    ~WidgetImageView();

    void init(QString sid);
    enum ZoomType { Fit, Fill, Identity, Value }; // List of possible zoom modes
    Q_ENUM(ZoomType);                   // Register enum names inside a QMetaObject

public slots:
    void onCoords();                    // Open coordinate dialog and center image on user input coordinates
    void syncView(QPointF pos, qreal zoom)
        { moveTo(pos); setZoom(zoom); } // Handle broadcast to sync all image views
    void setImage(uint img, bool blend);// Sets or blends an image layer by its index
    void setLayer(QString id)           //* Sets the layer id (“1”...”k”)
        { setImage(QString("123456789abcdefghijk").indexOf(id), false ); }
    void addLayer(QString id)           //* Adds the layer id (“1”...”k”) to the one(s) already set
        { setImage(QString("123456789abcdefghijk").indexOf(id), true ); }
    void setZoom(qreal);                //* Sets the zoom value
    void setPos(uint x, uint y)         //* Sets the image position
        { moveTo(QPointF(qreal(x) / m_image.width(), qreal(y) / m_image.height())); }
    void find(QString s) { onFind(s); } //* Finds and shows the named feature
    void show(uint x, uint y, uint w, uint h) //* Highlight a rectangle
        { m_r = QRect(x,y,w,h); m_highlight_trans = &m_r; m_timer_tick = 10; }
    void state();                       //* Prints the img view state
    void annot(QString fileName)        //* Loads custom annotation file
        { m_dropppedFile = fileName; dropEvent(nullptr); }

private slots:
    void onFind(QString text);          // Search for the named feature
    void onTimeout();                   // Timer timeout handler
    void contextMenu(const QPoint &pos);// Mouse context menu handler
    void editAnnotations();             // Opens dialog to edit annotations
    void addAnnotation();               // Adds a new annotation within the selected box and opens dialog to edit it
    void editTip();                     // Opens a dialog to edit a tip
    void netsDriving();                 // Shows nets that the selected net is driving
    void netsDriven();                  // Shows nets that drive the selected net
    void editNetName();                 // Opens dialog to edit selected net name (alias)
    void viewSchematic();               // Creates a new Schematic window using the selected net

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *) override;

private:
    Ui::WidgetImageView *ui;

    QImage  m_image;                    // Current image
    QSize   m_panelSize;                // View panel size, drawable area
    QPointF m_tex;                      // Texture coordinate to map to view center (normalized)
    qreal   m_scale;                    // Scaling value
    ZoomType m_view_mode;               // Current zoom mode

    QPoint  m_mousePos;                 // Current mouse position
    QPoint  m_pinMousePos;              // Mouse position at the time of button press
    bool    m_mouseLeftPressed {};      // Mouse left button is pressed
    bool    m_mouseRightPressed {};     // Mouse right button is pressed
    qreal   m_touchScale;               // Scale correction factor during a pinch-to-zoom event
    bool    m_drawSelection {};         // Draw the mouse selected area
    QRect   m_areaRect;                 // Area rectangle to draw during the mouse selection

    QTransform m_tx;                    // Transformation matrix from normalized image to screen space
    QTransform m_invtx;                 // Transformation matrix from screen to normalized image space
    QRect   m_viewPort;                 // Bounding rectangle of the current screen view
    QQueue<qreal> m_perf;               // Helps calculate the rolling average of the painter's performance
    WidgetImageOverlay *m_ov {};        // Image overlay class
    QTimer  m_timer;                    // Image refresh timer updates image every 1/2 seconds to show highlight blink
    uint    m_timer_tick;               // Timer timeout tick counter

    const segvdef *m_highlight_segment {}; // Segment to highlight in the current image
    const QRect *m_highlight_trans {};  // Transistor bounding rectangle to highlight in the current image
    QRect m_r;                          // Rectangle used by the show() scripting command to highlight a rectangle
    bool m_drawActiveNets;              // Draw active nets
    uint m_drawActiveNetsOrder {};      // The order of drawing active nets bit[0], auto toggle bit[1]
    bool m_drawAnnotations;             // Draw image annotations
    bool m_drawTransistors;             // Draw transistors
    uint m_drawTransistorMode {};       // Draw transistors mode
    bool m_drawLatches;                 // Draw latches
    bool m_drawNetNames {true};         // Dynamically write nearby net names (experimental)
    QString m_dropppedFile;             // File name of the file being dropped by a drag-and-drop operation

    QVector<net_t> m_drivingNets;       // List of nets expanded by the driving/driven heuristic

    bool event(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void leaveEvent(QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

    void moveTo(QPointF);               // Moved the image in the pane to normalized coordinate
    void moveBy(QPointF);               // Moves the image in the pane by specified normalized delta
    void setZoomMode(ZoomType);         // Sets the view mode

    void updateInfoArea(QPoint pt);
    void calcTransform();
    void clampImageCoords(QPointF &tex, qreal xmax = 1.0, qreal ymax = 1.0);
    void createLayout();
};

#endif // WIDGETIMAGEVIEW_H
