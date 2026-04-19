#ifndef CLASSRENDERER_H
#define CLASSRENDERER_H

#include "AppTypes.h"
#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QStringList>
#include <QVector>

/*
 * Parameters controlling one offscreen die render. All coordinates are
 * pixels on the 4700x5000 master die image (the "texture" coordinate
 * system used by the rest of the app).
 */
struct RenderSpec
{
    QRectF worldRect;                   // Source region of the die to render
    QSize  outputSize { 1024, 1024 };   // PNG dimensions
    QStringList layerNames;             // e.g. {"diffusion", "polysilicon", "metal"}; empty -> default

    bool drawNets {true};               // Overlay net activity / colouring
    bool drawTransistors {false};       // Overlay transistor outlines (active/inactive)
    bool drawLatches {false};           // Overlay latch bounding boxes
    bool drawAnnotations {true};        // Overlay user text annotations

    QVector<net_t>  highlightNets;      // Nets to paint in bright yellow on top
    QVector<tran_t> highlightTrans;     // Transistors to paint in cyan on top
    QVector<QRect>  highlightRects;     // Arbitrary rectangles (for block overlays)

    uint netMode {0};                   // 0 active / 1 pullup / 2 gateless / 3 gateless no-pullup
    uint transistorMode {0};            // ClassVisual::drawTransistors mode passthrough
};

struct RenderResult
{
    QImage image;                       // ARGB premultiplied
    QByteArray png;                     // Lazily filled by encodePng()
    QRectF worldRect;                   // Actual world rect used (post-clamp)
};

/*
 * ClassRenderer — offscreen rendering of arbitrary die regions.
 *
 * Replicates the painting pipeline of WidgetImageView::paintEvent without
 * any widget dependency: blends the chosen chip layers into a composite
 * QImage, then asks ClassVisual to draw nets / transistors / latches and
 * ClassAnnotate to draw labels on top, all through a QPainter bound to
 * an offscreen QImage.
 *
 * Must be invoked on the main thread (Qt graphics is not thread-safe).
 * MCP handlers marshal through ClassMcpThreading before calling.
 */
class ClassRenderer : public QObject
{
    Q_OBJECT
public:
    explicit ClassRenderer(QObject *parent = nullptr);

    // Render a region of the die. On success result.image is valid.
    RenderResult renderRegion(const RenderSpec &spec);

    // Render the entire die into the given output size
    RenderResult renderFullDie(const QSize &outputSize, const QStringList &layerNames);

    // Encode a result's QImage into a PNG blob (cached on the result)
    static QByteArray encodePng(const QImage &img);

    // Helper: resolve a layer name (case-insensitive) to its image index, -1 if unknown
    int resolveLayerIndex(const QString &name) const;

    // Returns the list of available layer names as seen by this renderer
    QStringList availableLayers() const;

private:
    QImage composeBase(const QStringList &layerNames);
    void drawHighlights(QPainter &painter, const RenderSpec &spec, qreal scale);
};

#endif // CLASSRENDERER_H
