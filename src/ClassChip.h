#ifndef CLASSCHIP_H
#define CLASSCHIP_H

#include "AppTypes.h"
#include <QFont>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPainterPath>

// Contains visual definition of a transistor
struct transvdef
{
    tran_t id;                          // Transistor number
    net_t gatenet;                      // Net (segment) connected to its gate
    QRect box;                          // Rectangle where it is (roughly) located
    QPainterPath path;                  // Outline of the transistor topology as a single QPainter path
};

// Contains visual definition of a segment (paths connected together into a single trace)
struct segvdef
{
    net_t netnum {};                    // A non-zero net number
    QVector<QPainterPath> paths {};     // Outline of the segment topology as a set of QPainter paths
};

// Contains information about a detected latch
struct latchdef
{
    tran_t t1, t2;                      // Two transistors that make up a latch
    net_t n1, n2;                       // Two nets that inter-connect into a latch
    QRect box;                          // Rectangle where it is (roughly) located
};

/*
 * ClassChip contains functions to hold the visual chip data and to manipulate with a set of die images,
 * some loaded and some generated by this class code
 */
class ClassChip: public QObject
{
    Q_OBJECT

public:
    ClassChip() {};

    bool loadChipResources(QString dir);// Attempts to load all expected chip resources
    QImage &getImage(uint i);           // Returns a reference to the image by the image index
    QImage &getImage(QString name, bool &ok); // Returns a reference to the image by the image (embedded) name

    template<bool includeVssVcc>
    const QVector<net_t> getNetsAt(int x, int y); // Returns a list of (unique) nets located at the specified image coordinates
    const QStringList getImageNames();    // Returns a list of layer / image names
    const segvdef *getSegment(net_t net); // Returns the segment visual definition, zero if not found
    void toggleAltSegdef()                // Toggle alternate segment definition as active
        { use_alt_segdef = !use_alt_segdef; }
    const transvdef *getTrans(tran_t id); // Returns transistor visual definition, nullptr if not found
    tran_t getTransistorAt(int x, int y); // Returns a transistor at the specified image coordinates
    bool isLatch(net_t net);              // Returns true if a net is part of any latch
    void getLatch(net_t net, tran_t &t1, tran_t &t2, net_t &n1, net_t &n2); // Returns latch transistors and nets

public slots:
    void detectLatches();               // Detects latches and also loads custom latch definitions
    void drawLatches(QPainter &painter, const QRect &viewport);
    void experimental(int n);           // Runs experimental function number n
    void expDrawTransistors(QPainter &painter, const QRect &viewport, bool highlightAll);
    void expDynamicallyNameNets(QPainter &painter, const QRect &viewport, qreal scale); // Maps nearby net names

private:
    QVector<transvdef> m_transvdefs;    // Array of transistor visual definitions
    QHash<net_t, segvdef> m_segvdefs;   // Hash of segment visual definitions, key is the segment net number
    QHash<net_t, segvdef> m_segvdefs2;  // Alternate segment visual definitions
    bool use_alt_segdef {false};        // Use alternate segment definitions
    QVector<latchdef> m_latches;        // Array of latches
    QVector<QImage> m_img;              // Chip layer images
    uint m_sx {};                       // X size of all images and maps
    uint m_sy {};                       // Y size of all images and maps
    uint m_mapsize;                     // Map size in bytes, equals to (m_sx * m_sy)
    uint16_t *m_p3[3] {};               // Layer map: [0] diffusion, [1] poly, [2] metal
    uchar *m_fmap {};                   // Feature bitmap

private:
    bool loadImages(QString dir);       // Loads chip images
    bool loadSegdefs(QString dir);      // Loads segment defintions
    bool loadSegdefsJs(QString dir);    // Loads segdefs.js
    bool loadTransdefs(QString dir);    // Loads transdefs.js
    void setFirstImage(QString name);   // Sets the given image to be the first one in m_img vector
    bool addTransistorsLayer();         // Inserts an image of the transistors layer
    void drawTransistors(QImage &img);  // Draws transistors on the given image surface
    bool convertToGrayscale();          // Converts loaded images to grayscale format
    bool loadLayerMap(QString dir);     // Loads layer map
    void buildFeatureMap();             // Builds the feature map from individual layer images of a die
    void shrinkVias(QString source, QString dest); // Creates a via layer with 1x1 vias
    void createLayerMapImage(QString name, bool onlyVssVcc); // Creates a color image from the layer map data
    void fill(const uchar *p_map, uint16_t x, uint16_t y, uint layer, uint16_t id);
    void drawFeature(uint16_t x, uint16_t y, uint layer, uint16_t id);
    void fillLayerMap();                // Fills layer map with vss and vcc
    void saveLayerMap();                // Saves layer map to a file
    // Experimental code
    void experimental_1();              // Merges net paths for a better visual display
    bool saveSegvdefs(QString dir);     // Saves m_segvdefs
    bool loadSegvdefs(QString dir);     // Loads m_segvdefs
    void drawAllNetsAsInactive(QString source, QString dest);
    void redrawNetsColorize(QString source, QString dest);
    bool loadLatches();                 // Helper to load custom latch definitions
    void experimental_3();              // Creates transistors paths hinted by transdef bounding boxes
    void experimental_4();              // Creates transistors paths based on our feature bitmap
    bool scanForTransistor(uchar const *p, QRect t, uint &x, uint &y);
    void edgeWalk(uchar const *p, QPainterPath &path, uint x, uint y);
    uint edgeWalkFindDir(uchar const *p, uint x, uint y, uint startDir);
    bool scanForTransistor_4(uchar const *p, uint &offset);
    QFont m_fixedFont { QFont("Consolas", 8) }; // Font for net names
};

#endif // CLASSCHIP_H
