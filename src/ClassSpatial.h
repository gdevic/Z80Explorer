#ifndef CLASSSPATIAL_H
#define CLASSSPATIAL_H

#include "AppTypes.h"
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

// A named functional region on the die (PLA, ALU, register file, ...).
// Loaded from resource/functional_blocks.json; user-editable.
struct FunctionalBlock
{
    QString name;                       // Short block name, e.g. "ALU"
    QString description;                // One-line human description
    QRect rect;                         // Bounding rectangle in die-pixel space (4700x5000 master)
    QStringList tags;                   // Free-form tags for search / categorisation
};

// Result of a nearest-neighbour query on a transistor or net
struct SpatialHit
{
    uint id {};                         // Transistor id or net id
    QRect bbox;                         // Bounding rectangle in die-pixel space
    qreal dist {};                      // Euclidean distance from query point to bbox centre
};

// Result of a bounding-box query over a set of ids
struct BBoxInfo
{
    QRect bbox;                         // Minimum bounding rectangle enclosing all inputs
    qreal compactness {};               // sum(area_i) / area(bbox); 1.0 = perfectly packed, tiny = scattered
    bool valid {false};                 // False if input was empty or no hits had valid boxes
};

// Result of a region_of query
struct RegionInfo
{
    QString blockName;                  // Name of the functional block containing the feature, empty if none
    QRect bbox;                         // Bounding rect of the feature itself
    QStringList nearbyAnnotations;      // Annotation labels within a fixed radius
    bool valid {false};
};

/*
 * ClassSpatial — transistor / net spatial index plus functional-block map.
 *
 * At startup (after ClassVisual has loaded chip resources), build() walks
 * m_transvdefs and m_segvdefs, indexing each bounding box into a coarse
 * grid hash. Lookups scan only the overlapping cells.
 *
 * functional_blocks.json (resource folder) names the big die regions the
 * user knows about (PLA, ALU, register file, ...). It is hand-authored
 * and user-edited; a small seed ships with the app.
 *
 * All public methods are safe to call from the main thread only; MCP
 * handlers marshal through ClassMcpThreading before invoking.
 */
class ClassSpatial : public QObject
{
    Q_OBJECT
public:
    explicit ClassSpatial(QObject *parent = nullptr);

    bool build(const QString &resourceDir);  // Populate spatial index and load functional_blocks.json
    bool reloadBlocks(const QString &resourceDir); // Reload functional_blocks.json without reindexing

    // Test-only: build the index directly from caller-supplied box tables,
    // bypassing ClassVisual. trans[i] is transistor i's box (empty = absent).
    // nets[i] is net i's bounding box. Does NOT touch the blocks list.
    void buildFromBoxes(const QVector<QRect> &trans, const QVector<QRect> &nets);
    void setBlocks(const QVector<FunctionalBlock> &blocks) { m_blocks = blocks; }

    // Returns transistor box if known, empty QRect otherwise
    QRect transBox(tran_t t) const;
    QRect netBBox(net_t n) const;

    QVector<SpatialHit> netsNear(qreal x, qreal y, qreal radius) const;
    QVector<SpatialHit> transNear(qreal x, qreal y, qreal radius) const;

    BBoxInfo boundingBoxOfNets(const QVector<net_t> &ids) const;
    BBoxInfo boundingBoxOfTrans(const QVector<tran_t> &ids) const;

    RegionInfo regionOfNet(net_t n) const;
    RegionInfo regionOfTrans(tran_t t) const;

    QString blockAt(qreal x, qreal y) const;
    QStringList annotationsNear(qreal x, qreal y, qreal radius) const;

    const QVector<FunctionalBlock> &blocks() const { return m_blocks; }
    int transCount() const { return m_transBoxes.size(); }
    int netBBoxCount() const { return m_netBBoxes.size(); }

private:
    static constexpr int DIE_W = 4700;
    static constexpr int DIE_H = 5000;
    static constexpr int GRID_CELL = 100;        // 47 x 50 coarse cells
    static constexpr int GRID_COLS = (DIE_W + GRID_CELL - 1) / GRID_CELL;
    static constexpr int GRID_ROWS = (DIE_H + GRID_CELL - 1) / GRID_CELL;

    // Each grid cell lists the transistor / net ids whose box overlaps that cell
    QVector<QVector<tran_t>> m_transGrid;   // size = GRID_ROWS * GRID_COLS
    QVector<QVector<net_t>>  m_netGrid;
    QVector<QRect> m_transBoxes;            // m_transBoxes[t] = box (empty if unknown)
    QVector<QRect> m_netBBoxes;             // m_netBBoxes[n]  = bbox (empty if unknown)
    QVector<FunctionalBlock> m_blocks;

    bool loadBlocks(const QString &path);
    void indexRect(int row, int col, QVector<QVector<tran_t>> &grid, tran_t id);
    void indexNet(int row, int col, QVector<QVector<net_t>> &grid, net_t id);
    inline int cellIndex(int row, int col) const { return row * GRID_COLS + col; }
    static qreal dist(const QRect &r, qreal x, qreal y);
};

#endif // CLASSSPATIAL_H
