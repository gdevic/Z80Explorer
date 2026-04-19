#include "ClassSpatial.h"
#include "ClassAnnotate.h"
#include "ClassController.h"
#include "ClassVisual.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
#include <algorithm>

ClassSpatial::ClassSpatial(QObject *parent)
    : QObject(parent)
{
}

/*
 * Build the spatial index from ClassVisual's transistor and segment
 * visual definitions. Called once at startup after initChip() completes.
 */
bool ClassSpatial::build(const QString &resourceDir)
{
    m_transGrid.fill({}, GRID_ROWS * GRID_COLS);
    m_netGrid.fill({}, GRID_ROWS * GRID_COLS);
    m_transBoxes.fill(QRect(), MAX_TRANS);
    m_netBBoxes.fill(QRect(), MAX_NETS);

    ClassVisual &chip = ::controller.getChip();

    uint indexedTrans = 0;
    for (tran_t t = 1; t < MAX_TRANS; t++)
    {
        const transvdef *tv = chip.getTrans(t);
        if (!tv || tv->box.isEmpty())
            continue;
        m_transBoxes[t] = tv->box;
        const int c0 = qBound(0, tv->box.left()   / GRID_CELL, GRID_COLS - 1);
        const int c1 = qBound(0, tv->box.right()  / GRID_CELL, GRID_COLS - 1);
        const int r0 = qBound(0, tv->box.top()    / GRID_CELL, GRID_ROWS - 1);
        const int r1 = qBound(0, tv->box.bottom() / GRID_CELL, GRID_ROWS - 1);
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++)
                m_transGrid[cellIndex(r, c)].append(t);
        indexedTrans++;
    }

    uint indexedNets = 0;
    for (net_t n = 1; n < MAX_NETS; n++)
    {
        const segvdef *sv = chip.getSegment(n);
        if (!sv || sv->path.isEmpty())
            continue;
        QRect box = sv->path.boundingRect().toAlignedRect();
        if (box.isEmpty())
            continue;
        m_netBBoxes[n] = box;
        const int c0 = qBound(0, box.left()   / GRID_CELL, GRID_COLS - 1);
        const int c1 = qBound(0, box.right()  / GRID_CELL, GRID_COLS - 1);
        const int r0 = qBound(0, box.top()    / GRID_CELL, GRID_ROWS - 1);
        const int r1 = qBound(0, box.bottom() / GRID_CELL, GRID_ROWS - 1);
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++)
                m_netGrid[cellIndex(r, c)].append(n);
        indexedNets++;
    }

    loadBlocks(resourceDir + "/functional_blocks.json");

    qInfo() << "ClassSpatial: indexed" << indexedTrans << "transistors and" << indexedNets
            << "nets;" << m_blocks.count() << "functional blocks loaded";
    return true;
}

bool ClassSpatial::reloadBlocks(const QString &resourceDir)
{
    return loadBlocks(resourceDir + "/functional_blocks.json");
}

/*
 * Test entry point: populate the grid directly from supplied box tables
 * rather than reading them from ClassVisual. Does not touch m_blocks.
 */
void ClassSpatial::buildFromBoxes(const QVector<QRect> &trans, const QVector<QRect> &nets)
{
    m_transGrid.fill({}, GRID_ROWS * GRID_COLS);
    m_netGrid.fill({}, GRID_ROWS * GRID_COLS);
    m_transBoxes = trans;
    if (m_transBoxes.size() < MAX_TRANS) m_transBoxes.resize(MAX_TRANS);
    m_netBBoxes  = nets;
    if (m_netBBoxes.size() < MAX_NETS)   m_netBBoxes.resize(MAX_NETS);

    for (int t = 0; t < m_transBoxes.size() && t < MAX_TRANS; t++)
    {
        const QRect &b = m_transBoxes[t];
        if (b.isEmpty()) continue;
        const int c0 = qBound(0, b.left()   / GRID_CELL, GRID_COLS - 1);
        const int c1 = qBound(0, b.right()  / GRID_CELL, GRID_COLS - 1);
        const int r0 = qBound(0, b.top()    / GRID_CELL, GRID_ROWS - 1);
        const int r1 = qBound(0, b.bottom() / GRID_CELL, GRID_ROWS - 1);
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++)
                m_transGrid[cellIndex(r, c)].append(tran_t(t));
    }
    for (int n = 0; n < m_netBBoxes.size() && n < MAX_NETS; n++)
    {
        const QRect &b = m_netBBoxes[n];
        if (b.isEmpty()) continue;
        const int c0 = qBound(0, b.left()   / GRID_CELL, GRID_COLS - 1);
        const int c1 = qBound(0, b.right()  / GRID_CELL, GRID_COLS - 1);
        const int r0 = qBound(0, b.top()    / GRID_CELL, GRID_ROWS - 1);
        const int r1 = qBound(0, b.bottom() / GRID_CELL, GRID_ROWS - 1);
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++)
                m_netGrid[cellIndex(r, c)].append(net_t(n));
    }
}

bool ClassSpatial::loadBlocks(const QString &path)
{
    m_blocks.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qInfo() << "ClassSpatial: no functional_blocks.json at" << path << "- continuing without blocks";
        return false;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError)
    {
        qWarning() << "ClassSpatial: functional_blocks.json parse error:" << err.errorString();
        return false;
    }
    QJsonArray arr = doc.object().value("blocks").toArray();
    for (const QJsonValue &v : arr)
    {
        QJsonObject o = v.toObject();
        FunctionalBlock b;
        b.name = o.value("name").toString();
        b.description = o.value("description").toString();
        QJsonArray r = o.value("rect").toArray();
        if (r.size() == 4)
            b.rect = QRect(r[0].toInt(), r[1].toInt(), r[2].toInt(), r[3].toInt());
        QJsonArray tags = o.value("tags").toArray();
        for (const QJsonValue &tv : tags)
            b.tags.append(tv.toString());
        if (!b.name.isEmpty() && !b.rect.isEmpty())
            m_blocks.append(b);
    }
    return true;
}

QRect ClassSpatial::transBox(tran_t t) const
{
    return (t < m_transBoxes.size()) ? m_transBoxes[t] : QRect();
}

QRect ClassSpatial::netBBox(net_t n) const
{
    return (n < m_netBBoxes.size()) ? m_netBBoxes[n] : QRect();
}

qreal ClassSpatial::dist(const QRect &r, qreal x, qreal y)
{
    const qreal cx = r.left() + r.width()  * 0.5;
    const qreal cy = r.top()  + r.height() * 0.5;
    const qreal dx = cx - x;
    const qreal dy = cy - y;
    return std::sqrt(dx * dx + dy * dy);
}

QVector<SpatialHit> ClassSpatial::transNear(qreal x, qreal y, qreal radius) const
{
    QVector<SpatialHit> out;
    if (radius <= 0) return out;
    const int cMin = qBound(0, int((x - radius) / GRID_CELL), GRID_COLS - 1);
    const int cMax = qBound(0, int((x + radius) / GRID_CELL), GRID_COLS - 1);
    const int rMin = qBound(0, int((y - radius) / GRID_CELL), GRID_ROWS - 1);
    const int rMax = qBound(0, int((y + radius) / GRID_CELL), GRID_ROWS - 1);
    QVector<bool> seen(MAX_TRANS, false);
    for (int r = rMin; r <= rMax; r++)
        for (int c = cMin; c <= cMax; c++)
            for (tran_t t : m_transGrid[cellIndex(r, c)])
            {
                if (seen[t]) continue;
                seen[t] = true;
                const QRect &box = m_transBoxes[t];
                qreal d = dist(box, x, y);
                if (d <= radius)
                    out.append(SpatialHit { t, box, d });
            }
    std::sort(out.begin(), out.end(), [](const SpatialHit &a, const SpatialHit &b) { return a.dist < b.dist; });
    return out;
}

QVector<SpatialHit> ClassSpatial::netsNear(qreal x, qreal y, qreal radius) const
{
    QVector<SpatialHit> out;
    if (radius <= 0) return out;
    const int cMin = qBound(0, int((x - radius) / GRID_CELL), GRID_COLS - 1);
    const int cMax = qBound(0, int((x + radius) / GRID_CELL), GRID_COLS - 1);
    const int rMin = qBound(0, int((y - radius) / GRID_CELL), GRID_ROWS - 1);
    const int rMax = qBound(0, int((y + radius) / GRID_CELL), GRID_ROWS - 1);
    QVector<bool> seen(MAX_NETS, false);
    for (int r = rMin; r <= rMax; r++)
        for (int c = cMin; c <= cMax; c++)
            for (net_t n : m_netGrid[cellIndex(r, c)])
            {
                if (seen[n]) continue;
                seen[n] = true;
                const QRect &box = m_netBBoxes[n];
                qreal d = dist(box, x, y);
                if (d <= radius)
                    out.append(SpatialHit { n, box, d });
            }
    std::sort(out.begin(), out.end(), [](const SpatialHit &a, const SpatialHit &b) { return a.dist < b.dist; });
    return out;
}

BBoxInfo ClassSpatial::boundingBoxOfTrans(const QVector<tran_t> &ids) const
{
    BBoxInfo info;
    QRect bbox;
    qreal areaSum = 0;
    for (tran_t t : ids)
    {
        if (t >= m_transBoxes.size()) continue;
        const QRect &box = m_transBoxes[t];
        if (box.isEmpty()) continue;
        bbox = bbox.isEmpty() ? box : bbox.united(box);
        areaSum += qreal(box.width()) * box.height();
    }
    if (bbox.isEmpty()) return info;
    info.bbox = bbox;
    const qreal bboxArea = qreal(bbox.width()) * bbox.height();
    info.compactness = bboxArea > 0 ? areaSum / bboxArea : 0;
    info.valid = true;
    return info;
}

BBoxInfo ClassSpatial::boundingBoxOfNets(const QVector<net_t> &ids) const
{
    BBoxInfo info;
    QRect bbox;
    qreal areaSum = 0;
    for (net_t n : ids)
    {
        if (n >= m_netBBoxes.size()) continue;
        const QRect &box = m_netBBoxes[n];
        if (box.isEmpty()) continue;
        bbox = bbox.isEmpty() ? box : bbox.united(box);
        areaSum += qreal(box.width()) * box.height();
    }
    if (bbox.isEmpty()) return info;
    info.bbox = bbox;
    const qreal bboxArea = qreal(bbox.width()) * bbox.height();
    info.compactness = bboxArea > 0 ? areaSum / bboxArea : 0;
    info.valid = true;
    return info;
}

QString ClassSpatial::blockAt(qreal x, qreal y) const
{
    // Pick the smallest block containing the point (more specific wins)
    QString best;
    qint64 bestArea = -1;
    for (const FunctionalBlock &b : m_blocks)
    {
        if (b.rect.contains(int(x), int(y)))
        {
            const qint64 a = qint64(b.rect.width()) * b.rect.height();
            if (bestArea < 0 || a < bestArea)
            {
                bestArea = a;
                best = b.name;
            }
        }
    }
    return best;
}

QStringList ClassSpatial::annotationsNear(qreal x, qreal y, qreal radius) const
{
    QStringList out;
    const QVector<annotation> &list = ::controller.getAnnotation().get();
    const qreal r2 = radius * radius;
    for (const annotation &a : list)
    {
        const qreal cx = a.rect.left() + a.rect.width()  * 0.5;
        const qreal cy = a.rect.top()  + a.rect.height() * 0.5;
        const qreal dx = cx - x;
        const qreal dy = cy - y;
        if (dx * dx + dy * dy <= r2)
            out.append(a.text);
    }
    return out;
}

RegionInfo ClassSpatial::regionOfNet(net_t n) const
{
    RegionInfo r;
    if (n >= m_netBBoxes.size() || m_netBBoxes[n].isEmpty())
        return r;
    r.bbox = m_netBBoxes[n];
    const qreal cx = r.bbox.left() + r.bbox.width()  * 0.5;
    const qreal cy = r.bbox.top()  + r.bbox.height() * 0.5;
    r.blockName = blockAt(cx, cy);
    r.nearbyAnnotations = annotationsNear(cx, cy, 200.0);
    r.valid = true;
    return r;
}

RegionInfo ClassSpatial::regionOfTrans(tran_t t) const
{
    RegionInfo r;
    if (t >= m_transBoxes.size() || m_transBoxes[t].isEmpty())
        return r;
    r.bbox = m_transBoxes[t];
    const qreal cx = r.bbox.left() + r.bbox.width()  * 0.5;
    const qreal cy = r.bbox.top()  + r.bbox.height() * 0.5;
    r.blockName = blockAt(cx, cy);
    r.nearbyAnnotations = annotationsNear(cx, cy, 200.0);
    r.valid = true;
    return r;
}
