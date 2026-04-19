#include "ClassRenderer.h"
#include "ClassAnnotate.h"
#include "ClassController.h"
#include "ClassVisual.h"
#include <QBuffer>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

ClassRenderer::ClassRenderer(QObject *parent)
    : QObject(parent)
{
}

/*
 * Returns the image index (as used by ClassVisual::getImage(uint)) that
 * matches the given case-insensitive layer name. The renderer accepts
 * a small vocabulary of familiar names that map onto the actual image
 * names ClassVisual reports via getImageNames().
 */
int ClassRenderer::resolveLayerIndex(const QString &name) const
{
    QStringList names = ::controller.getChip().getImageNames();
    // Build normalised map (lowercase, strip extensions)
    for (int i = 0; i < names.size(); i++)
    {
        QString n = names[i].toLower();
        QString wanted = name.toLower();
        if (n == wanted || n.contains(wanted) || wanted.contains(n))
            return i;
    }
    // Accept common aliases
    static const QVector<QPair<QString, QStringList>> aliases = {
        { "diffusion",   { "diff", "diffusion" } },
        { "polysilicon", { "poly", "polysilicon" } },
        { "metal",       { "metal" } },
        { "vias",        { "vias", "via" } },
        { "buried",      { "buried" } },
        { "ions",        { "ions", "ion" } },
        { "nets",        { "nets" } },
        { "transistors", { "trans", "transistors" } },
    };
    for (const auto &p : aliases)
    {
        if (p.second.contains(name.toLower()))
        {
            for (int i = 0; i < names.size(); i++)
                if (names[i].toLower().contains(p.first))
                    return i;
        }
    }
    return -1;
}

QStringList ClassRenderer::availableLayers() const
{
    return ::controller.getChip().getImageNames();
}

/*
 * Build the composite base image by starting from the first requested
 * layer and XOR-blending each additional layer, matching the widget's
 * setImage(img, true) semantics.
 */
QImage ClassRenderer::composeBase(const QStringList &layerNames)
{
    ClassVisual &chip = ::controller.getChip();
    QStringList resolved = layerNames;
    if (resolved.isEmpty())
    {
        // Default: the widget's own default "nets" base view if available
        QStringList available = chip.getImageNames();
        if (!available.isEmpty())
            resolved.append(available.first());
    }

    QImage base;
    bool first = true;
    for (const QString &name : resolved)
    {
        int idx = resolveLayerIndex(name);
        if (idx < 0)
            continue;
        QImage &layer = chip.getImage(uint(idx));
        if (layer.isNull())
            continue;
        if (first)
        {
            base = layer.copy(); // deep copy — we'll paint on top
            first = false;
        }
        else
        {
            QPainter p(&base);
            p.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
            p.drawImage(0, 0, layer);
        }
    }
    return base;
}

RenderResult ClassRenderer::renderRegion(const RenderSpec &spec)
{
    RenderResult out;

    // Compose the source image (die-space)
    QImage composite = composeBase(spec.layerNames);
    if (composite.isNull())
    {
        qWarning() << "ClassRenderer: no layers composited; rendering blank";
        out.image = QImage(spec.outputSize, QImage::Format_ARGB32_Premultiplied);
        out.image.fill(Qt::black);
        return out;
    }

    // Clamp world rect to the die bounds
    QRectF world = spec.worldRect;
    const QRectF die(0, 0, composite.width(), composite.height());
    if (world.isEmpty())
        world = die;
    else
        world = world.intersected(die);
    if (world.isEmpty())
    {
        out.image = QImage(spec.outputSize, QImage::Format_ARGB32_Premultiplied);
        out.image.fill(Qt::black);
        return out;
    }
    out.worldRect = world;

    // Allocate output image
    out.image = QImage(spec.outputSize, QImage::Format_ARGB32_Premultiplied);
    out.image.fill(Qt::white);

    QPainter painter(&out.image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Build transform: map world rect into output rect
    const qreal sx = qreal(spec.outputSize.width())  / world.width();
    const qreal sy = qreal(spec.outputSize.height()) / world.height();
    QTransform tx;
    tx.scale(sx, sy);
    tx.translate(-world.left(), -world.top());
    painter.setTransform(tx);

    // Draw the composite base image
    painter.drawImage(QPointF(0, 0), composite);

    // Viewport rectangle for cull logic (in texture / die space)
    const QRect viewportTex = world.toAlignedRect();

    ClassVisual &chip = ::controller.getChip();

    // Net colouring overlay
    if (spec.drawNets)
    {
        painter.save();
        chip.drawNets(painter, viewportTex, /*order*/ false, /*mode*/ spec.netMode);
        painter.restore();
    }
    // Latch boxes
    if (spec.drawLatches)
    {
        painter.save();
        chip.drawLatches(painter, viewportTex, /*drawText*/ false);
        painter.restore();
    }
    // Transistor outlines
    if (spec.drawTransistors)
    {
        painter.save();
        chip.drawTransistors(painter, viewportTex, spec.transistorMode);
        painter.restore();
    }

    // Highlights
    drawHighlights(painter, spec, sx);

    // Annotations
    if (spec.drawAnnotations)
    {
        painter.save();
        ::controller.getAnnotation().draw(painter, viewportTex, sx);
        painter.restore();
    }
    // Latch labels on top
    if (spec.drawLatches)
    {
        painter.save();
        chip.drawLatches(painter, viewportTex, /*drawText*/ true);
        painter.restore();
    }

    painter.end();
    return out;
}

RenderResult ClassRenderer::renderFullDie(const QSize &outputSize, const QStringList &layerNames)
{
    RenderSpec spec;
    spec.worldRect = QRectF(0, 0, 4700, 5000);
    spec.outputSize = outputSize;
    spec.layerNames = layerNames;
    spec.drawNets = false; // full-die nets would be noisy
    spec.drawTransistors = false;
    spec.drawAnnotations = true;
    return renderRegion(spec);
}

void ClassRenderer::drawHighlights(QPainter &painter, const RenderSpec &spec, qreal scale)
{
    ClassVisual &chip = ::controller.getChip();

    // Highlighted nets: bright yellow path fill
    if (!spec.highlightNets.isEmpty())
    {
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(QColor(255, 230, 0, 220), qMax(1.0, 2.0 / scale)));
        painter.setBrush(QColor(255, 255, 0, 160));
        for (net_t n : spec.highlightNets)
        {
            const segvdef *sv = chip.getSegment(n);
            if (sv)
                painter.drawPath(sv->path);
        }
        painter.restore();
    }

    // Highlighted transistors: cyan outlined box
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

    // Highlighted rectangles (e.g. block overlays)
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
