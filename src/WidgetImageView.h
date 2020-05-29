#ifndef WIDGETIMAGEVIEW_H
#define WIDGETIMAGEVIEW_H

#include "AppTypes.h"
#include <QTimer>
#include <QWidget>
#include <QQueue>

class WidgetImageOverlay;
struct segvdef;

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
    void init();
    enum ZoomType { Fit, Fill, Identity, Value }; // List of possible zoom modes
    Q_ENUM(ZoomType);                   // Register enum names inside a QMetaObject

public slots:
    void onCoords();                    // Open coordinate dialog and center image on user input coordinates
    void setImage(int i);               // Sets the image by its index, also considers Ctrl key to blend images
    void syncView(QPointF pos, qreal zoom)
        { moveTo(pos); setZoom(zoom); } // Handle broadcast to sync all image views

private slots:
    void onFind(QString text);          // Search for the named feature
    void onTimeout();                   // Timer timeout handler
    void onRunStopped(uint);            // Called by the sim when the current run stops at a given half-cycle
    void contextMenu(const QPoint &pos);// Mouse context menu handler
    void editAnnotations();             // Opens dialog to edit annotations
    void addAnnotation();               // Adds a new annotation within the selected box and opens dialog to edit it
    void editTip();                     // Opens a dialog to edit a tip
    void netsDriving();                 // Shows nets that the selected net is driving
    void netsDriven();                  // Shows nets that drive the selected net
    void editNetName();                 // Opens dialog to edit selected net name (alias)

private:
    Ui::WidgetImageView *ui;

    QImage  m_image;                    // Current image
    QSize   m_panelSize;                // View panel size, drawable area
    QPointF m_tex;                      // Texture coordinate to map to view center (normalized)
    qreal   m_scale;                    // Scaling value
    ZoomType m_view_mode;               // Current zoom mode
    bool    m_drawActiveNets {true};    // Draw active nets

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
    QRectF  m_imageView;                // Bounding rectangle in the texture space of the current screen viewport
    QQueue<qreal> m_perf;               // Helps calculate the rolling average of the painter's performance
    WidgetImageOverlay *m_ov;           // Image overlay class
    QTimer  m_timer;                    // Image refresh timer updates image every 1/2 seconds to show highlight blink
    uint    m_timer_tick;               // Timer timeout tick counter
    bool    m_enable_ctrl {};           // Initial disable of Ctrl key until we have fully constructed our view

    const segvdef *m_highlight_segment {}; // Segment to highlight in the current image
    const QRect *m_highlight_trans {};  // Transistor bounding rectangle to highlight in the current image
    bool m_drawAnnotations {true};      // Draw image annotations
    bool m_drawActiveTransistors {true};// Draw currently active transistors
    bool m_drawAllTransistors {false};  // Draw all transistors (irrespective of their state)
    bool m_drawNetNames {true};         // Dynamically write nearby net names (experimental)

    QVector<net_t> m_drivingNets;       // List of nets expanded by the driving/driven heuristic

    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    bool event(QEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void leaveEvent(QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

    void moveTo(QPointF);               // Moved the image in the pane to normalized coordinate
    void moveBy(QPointF);               // Moves the image in the pane by specified normalized delta
    void setZoomMode(ZoomType);         // Set the view mode
    void setZoom(qreal);                // Set the zoom value

    void calcTransform();
    void clampImageCoords(QPointF &tex, qreal xmax = 1.0, qreal ymax = 1.0);
    void createLayout();
};

#endif // WIDGETIMAGEVIEW_H
