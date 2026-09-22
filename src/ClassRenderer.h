#ifndef CLASSRENDERER_H
#define CLASSRENDERER_H

#include "AppTypes.h"
#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <QVector>

/*
 * Parameters controlling one offscreen die render. All coordinates are pixels on the 4700x5000
 * master die image, the same space ClassVisual reports bounding boxes in and the space the MCP
 * z80_net_info / z80_trans_info tools return.
 */
struct RenderSpec
{
    QRectF worldRect;                   // Source region of the die to render
    QSize  outputSize { 1024, 1024 };   // Output dimensions in pixels
    QStringList layerNames;             // Layer names; the first is the base, the rest XOR over it

    bool drawNets {true};               // Overlay net activity
    bool drawTransistors {false};       // Overlay transistor outlines
    bool drawLatches {false};           // Overlay latch boxes, and their names on top
    bool drawPullups {false};           // Overlay pull-up symbols
    bool drawNetNames {false};          // Overlay net-name labels
    bool drawAnnotations {true};        // Overlay user text annotations

    QVector<net_t>  highlightNets;      // Painted bright yellow over everything
    QVector<tran_t> highlightTrans;     // Painted cyan
    QVector<QRect>  highlightRects;     // Arbitrary rectangles, dashed red
    QHash<net_t, QColor> netColors;     // Per-net colour overlay; see the note in drawNetColors()

    uint netMode {0};                   // 0 active, 1 pull-up, 2 gate-less, 3 gate-less no pull-up
    uint transistorMode {0};            // 0 active, 1 single-flip, 2 sticky, 3 all
    bool netOrder {false};              // Reverse the segment paint order, as the view's Z key does
};

struct RenderResult
{
    QImage image;                       // ARGB32 premultiplied
    QRectF worldRect;                   // The source rect actually used, after clamping to the die
    QStringList layersUsed;             // Layer names that resolved and contributed
    QString error;                      // Non-empty when the render could not be produced
    bool pullupsSkipped {false};        // drawPullups was asked for but its zoom gate suppressed it
    bool netNamesSkipped {false};       // drawNetNames was asked for but scale was below 1.5
};

/*
 * ClassRenderer — offscreen rendering of arbitrary die regions.
 *
 * Replicates the painting pipeline of WidgetImageView::paintEvent without any widget dependency,
 * and in the same pass order, since later passes overpaint earlier ones. The layer composite is
 * built for the requested window only: the die images are 4700x5000, so compositing the whole
 * thing to answer a 64x64 request would cost ~94 MB per layer per call.
 *
 * Must be invoked on the main thread — Qt graphics is not thread-safe, and the ClassVisual draw
 * helpers read live simulation state. MCP handlers marshal through ClassMcpThreading before
 * calling.
 */
class ClassRenderer : public QObject
{
    Q_OBJECT
public:
    explicit ClassRenderer(QObject *parent = nullptr);

    // Render a region of the die. On failure result.error says why and result.image is null.
    RenderResult renderRegion(const RenderSpec &spec);

    // Encode an image as PNG
    static QByteArray encodePng(const QImage &img);

    // Resolves a layer name to its ClassVisual image index, or -1 when it matches nothing. An
    // exact (case-insensitive) name wins; otherwise a substring match is accepted only when it is
    // unique, so "metal" cannot silently land on "bw.metal".
    int resolveLayerIndex(const QString &name) const;

    // The layer names ClassVisual reports, in image-index order
    QStringList availableLayers() const;

    // True when QPainter can use this layer as a paint target. Eight of the twenty layers are
    // Format_Grayscale8, which QPainter::begin() refuses; the compositor works around it by
    // drawing every layer into its own ARGB32 scratch instead of painting onto a layer.
    static bool layerPaintable(const QImage &img);

private:
    // Composites the requested layers, cropped to `world`, into a new ARGB32 image
    QImage composeBase(const QStringList &layerNames, const QRect &world, QStringList &used);

    void drawNetColors(QPainter &painter, const RenderSpec &spec, const QRect &viewport, qreal scale);
    void drawHighlights(QPainter &painter, const RenderSpec &spec, qreal scale);
};

#endif // CLASSRENDERER_H
