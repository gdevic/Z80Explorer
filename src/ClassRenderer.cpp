#include "ClassRenderer.h"
#include "ClassAnnotate.h"
#include "ClassController.h"
#include "ClassVisual.h"
#include <QBuffer>
#include <QPainter>
#include <QPainterPath>

// Below this scale ClassVisual::expDynamicallyNameNets() returns without drawing, so asking for
// labels at a smaller scale silently produces none. The renderer reports that rather than leaving
// the caller to wonder where the labels went.
static const qreal NET_NAME_MIN_SCALE = 1.5;

ClassRenderer::ClassRenderer(QObject *parent)
    : QObject(parent)
{
}

bool ClassRenderer::layerPaintable(const QImage &img)
{
    const QImage::Format f = img.format();
    return (f != QImage::Format_Grayscale8) && (f != QImage::Format_Grayscale16)
        && (f != QImage::Format_Mono) && (f != QImage::Format_MonoLSB) && (f != QImage::Format_Invalid);
}

QStringList ClassRenderer::availableLayers() const
{
    return ::controller.getChip().getImageNames();
}

int ClassRenderer::resolveLayerIndex(const QString &name) const
{
    const QStringList names = ::controller.getChip().getImageNames();
    const QString wanted = name.trimmed().toLower();
    if (wanted.isEmpty())
        return -1;

    for (int i = 0; i < names.size(); i++)
    {
        if (names.at(i).toLower() == wanted)
            return i;
    }
    // A substring is accepted only when exactly one layer matches. Several names are prefixes of
    // others ("metal" of "bw.metal", "vss.vcc.nets" of "vss.vcc.nets.col"), and the old two-way
    // `contains` test resolved those to whichever came first in the vector.
    int hit = -1;
    int count = 0;
    for (int i = 0; i < names.size(); i++)
    {
        if (names.at(i).toLower().contains(wanted))
        {
            hit = i;
            count++;
        }
    }
    return (count == 1) ? hit : -1;
}

/*
 * Builds the base image for `world` by drawing the first layer into an ARGB32 scratch and
 * XOR-blending each further layer over it, which is what WidgetImageView::setImage(img, true)
 * does on screen.
 *
 * Two things drive the shape of this. Eight of the twenty layers are Format_Grayscale8 and
 * QPainter cannot paint onto those, so the scratch is always ARGB32 and the layers are only ever
 * read. And the source-rect form of drawImage() reads just the requested window, so the cost is
 * proportional to what was asked for rather than to the 4700x5000 die.
 */
QImage ClassRenderer::composeBase(const QStringList &layerNames, const QRect &world, QStringList &used)
{
    ClassVisual &chip = ::controller.getChip();
    QStringList resolved = layerNames;
    if (resolved.isEmpty())
        resolved.append("vss.vcc.nets.col");   // the view's own default base layer

    QImage base(world.size(), QImage::Format_ARGB32_Premultiplied);
    base.fill(Qt::transparent);
    QPainter p(&base);
    bool any = false;
    for (const QString &name : resolved)
    {
        const int idx = resolveLayerIndex(name);
        if (idx < 0)
            continue;
        QImage &layer = chip.getImage(uint(idx));
        if (layer.isNull())
            continue;
        p.setCompositionMode(any ? QPainter::RasterOp_SourceXorDestination
                                 : QPainter::CompositionMode_SourceOver);
        p.drawImage(QPoint(0, 0), layer, world);
        used.append(chip.getImageNames().at(idx));
        any = true;
    }
    p.end();
    if (!any)
        return QImage();
    return base;
}

RenderResult ClassRenderer::renderRegion(const RenderSpec &spec)
{
    RenderResult out;
    ClassVisual &chip = ::controller.getChip();

    const QImage &die0 = chip.getImage(0);
    if (die0.isNull())
    {
        out.error = "Chip layer images are not loaded";
        return out;
    }

    const QRectF die(0, 0, die0.width(), die0.height());
    QRectF world = spec.worldRect.isEmpty() ? die : spec.worldRect.intersected(die);
    if ((world.width() < 1) || (world.height() < 1))
    {
        out.error = QString("Requested region %1,%2 %3x%4 does not overlap the %5x%6 die")
                        .arg(spec.worldRect.left()).arg(spec.worldRect.top())
                        .arg(spec.worldRect.width()).arg(spec.worldRect.height())
                        .arg(die0.width()).arg(die0.height());
        return out;
    }
    if ((spec.outputSize.width() < 1) || (spec.outputSize.height() < 1))
    {
        out.error = "Output size must be at least one pixel on each side";
        return out;
    }
    out.worldRect = world;

    const QRect worldPix = world.toAlignedRect();
    QImage composite = composeBase(spec.layerNames, worldPix, out.layersUsed);
    if (composite.isNull())
    {
        out.error = QString("None of the requested layers resolved: %1. Call z80_die_info for the "
                            "layer names.").arg(spec.layerNames.join(", "));
        return out;
    }

    out.image = QImage(spec.outputSize, QImage::Format_ARGB32_Premultiplied);
    out.image.fill(Qt::transparent);

    QPainter painter(&out.image);
    const qreal sx = qreal(spec.outputSize.width())  / world.width();
    const qreal sy = qreal(spec.outputSize.height()) / world.height();
    // Smoothing helps when shrinking. Magnifying wants nearest-neighbour: the features being read
    // here are one or two die pixels wide and interpolation smears them into each other.
    painter.setRenderHint(QPainter::SmoothPixmapTransform, (sx < 1.0) || (sy < 1.0));

    // The composite covers worldPix, so it is drawn at that origin under the same transform as
    // every overlay below. That keeps one coordinate system for the whole pass list.
    QTransform tx;
    tx.scale(sx, sy);
    tx.translate(-world.left(), -world.top());
    painter.setTransform(tx);
    painter.drawImage(worldPix.topLeft(), composite);

    // Pass order mirrors WidgetImageView::paintEvent, because later passes overpaint earlier ones
    const QRect viewport = worldPix;

    if (spec.drawNets)
    {
        painter.save();
        chip.drawNets(painter, viewport, spec.netOrder, spec.netMode);
        painter.restore();
    }
    if (spec.drawLatches)
    {
        painter.save();
        chip.drawLatches(painter, viewport, false);
        painter.restore();
    }
    if (!spec.netColors.isEmpty())
        drawNetColors(painter, spec, viewport, sx);
    if (spec.drawTransistors)
    {
        // Single-flip and sticky count state changes, and the counters only advance once armed
        if ((spec.transistorMode == 1) || (spec.transistorMode == 2))
            chip.armTransFlipCount();
        painter.save();
        chip.drawTransistors(painter, viewport, spec.transistorMode);
        painter.restore();
    }
    if (spec.drawPullups)
    {
        painter.save();
        chip.drawPullups(painter, viewport);
        painter.restore();
        out.pullupsSkipped = (chip.getPullupCount() > 0) && (sx < 1.0);
    }

    drawHighlights(painter, spec, sx);

    if (spec.drawNetNames)
    {
        painter.save();
        chip.expDynamicallyNameNets(painter, viewport, sx);
        painter.restore();
        out.netNamesSkipped = (sx < NET_NAME_MIN_SCALE);
    }
    if (spec.drawAnnotations)
    {
        painter.save();
        ::controller.getAnnotation().draw(painter, viewport, sx);
        painter.restore();
    }
    if (spec.drawLatches)
    {
        painter.save();
        chip.drawLatches(painter, viewport, true);    // the names, on top of everything
        painter.restore();
    }

    painter.end();
    return out;
}

/*
 * Per-net colour overlay.
 *
 * This cannot be done through ClassVisual::drawNets(): that paints every active net with the one
 * ClassColors::getActive() brush and never consults the per-net colour rules. On screen, custom
 * net colours arrive only through the pre-rendered "vss.vcc.nets.col" layer, which is rebuilt when
 * a net is renamed. So a caller asking for "net 825 in red" needs its own pass, which is this one.
 *
 * Filled and then outlined, the way redrawNetsColorize() does it, so that two coloured nets lying
 * against each other stay distinguishable. The pen width is divided by the scale to keep the
 * outline one output pixel wide at any zoom.
 */
void ClassRenderer::drawNetColors(QPainter &painter, const RenderSpec &spec, const QRect &viewport,
                                  qreal scale)
{
    ClassVisual &chip = ::controller.getChip();
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for (auto it = spec.netColors.cbegin(); it != spec.netColors.cend(); ++it)
    {
        const segvdef *sv = chip.getSegment(it.key());
        // getSegment() returns a non-null but empty record for a net with no geometry
        if (!sv || sv->path.isEmpty())
            continue;
        if (!QRectF(viewport).intersects(sv->path.boundingRect()))
            continue;
        painter.setPen(Qt::NoPen);
        painter.setBrush(it.value());
        painter.drawPath(sv->path);
        painter.setPen(QPen(Qt::black, qMax(1.0, 1.0 / scale)));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(sv->path);
    }
    painter.restore();
}

void ClassRenderer::drawHighlights(QPainter &painter, const RenderSpec &spec, qreal scale)
{
    ClassVisual &chip = ::controller.getChip();

    if (!spec.highlightNets.isEmpty())
    {
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(QColor(255, 230, 0, 220), qMax(1.0, 2.0 / scale)));
        painter.setBrush(QColor(255, 255, 0, 160));
        for (net_t n : spec.highlightNets)
        {
            const segvdef *sv = chip.getSegment(n);
            if (sv && !sv->path.isEmpty())
                painter.drawPath(sv->path);
        }
        painter.restore();
    }

    if (!spec.highlightTrans.isEmpty())
    {
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(QColor(0, 230, 230, 220), qMax(1.0, 3.0 / scale)));
        painter.setBrush(QColor(0, 200, 200, 120));
        for (tran_t t : spec.highlightTrans)
        {
            const transvdef *tv = chip.getTrans(t);
            if (tv)
                painter.drawRect(tv->box);
        }
        painter.restore();
    }

    if (!spec.highlightRects.isEmpty())
    {
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(QColor(255, 80, 80, 200), qMax(1.5, 4.0 / scale), Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        for (const QRect &r : spec.highlightRects)
            painter.drawRect(r);
        painter.restore();
    }
}

QByteArray ClassRenderer::encodePng(const QImage &img)
{
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return out;
}
