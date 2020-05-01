#ifndef CLASSCHIP_H
#define CLASSCHIP_H

#include "AppTypes.h"
#include "ClassAnnotate.h"
#include "ClassTip.h"
#include <QImage>
#include <QHash>

// Contains visual definition of a segment (wire at the same voltage level)
struct segdef
{
    net_t nodenum {};               // A non-zero net number
    QVector<QPainterPath> paths {}; // Outline of the segment topology as a set of QPainter paths
};

// Contains individual transistor definition
struct transdef
{
    QString name;       // Transistor name (ex. 't251')
    net_t gatenode;     // Node (segment) connected to its gate
    QRect box;          // Visual rectangle where it is (roughly) located
    QPainterPath path;  // Outline of the transistor topology as a single QPainter path
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
    ClassAnnotate annotate;             // ClassChip "has-a" ClassAnnotate
    ClassTip tips;                      // ClassChip "has-a" ClassTip

    bool loadChipResources(QString dir);// Attempts to load all expected chip resources
    QImage &getImage(uint i);           // Returns a reference to the image by the image index
    QImage &getImage(QString name, bool &ok); // Returns a reference to the image by the image (embedded) name

    template<bool includeVssVcc>
    const QVector<net_t> getNetsAt(int x, int y); // Returns a list of (unique) nets located at the specified image coordinates
    const QString getTransistorAt(int x, int y); // Returns a transistor found at the specified image coordinates
    const QStringList getImageNames();  // Returns a list of layer / image names
    const segdef *getSegment(net_t net); // Returns the segdef given its net number, zero if not found
    const transdef *getTrans(QString name); // Returns transistor definition given its name, nullptr if not found

public slots:
    void experimental(int n);           // Runs experimental function number n
    void expDrawTransistors(QPainter &painter, const QRect &viewport, bool highlightAll);
    void expDynamicallyNameNets(QPainter &painter, const QRect &viewport, qreal scale); // Maps nearby net names

private:
    QHash<net_t, segdef> m_segdefs;     // Hash of segment definitions, key is the segment net number
    QVector<transdef> m_transdefs;      // Array of transistor definitions
    QVector<QImage> m_img;              // Chip layer images
    uint m_sx {};                       // X size of all images and maps
    uint m_sy {};                       // Y size of all images and maps
    uint m_mapsize;                     // Map size in bytes, equals to (m_sx * m_sy)
    uint16_t *m_p3[3] {};               // Layer map: [0] diffusion, [1] poly, [2] metal

private:
    bool loadImages(QString dir);       // Loads chip images
    bool loadSegdefs(QString dir);      // Loads segdefs.js
    bool loadTransdefs(QString dir);    // Loads transdefs.js
    void setFirstImage(QString name);   // Sets the given image to be the first one in m_img vector
    bool addTransistorsLayer();         // Inserts an image of the transistors layer
    void drawTransistors(QImage &img);  // Draws transistors on the given image surface
    bool convertToGrayscale();          // Converts loaded images to grayscale format
    bool loadLayerMap(QString dir);     // Loads layer map
    void buildFeatureMap();             // Builds the feature map from individual layer images of a die
    void shrinkVias(QString source, QString dest); // Creates a via layer with 1x1 vias
    void createLayerMapImage(QString name, bool onlyVssVcc); // Creates a color image from the layer map data
    // Experimental code
    void fill(const uchar *p_map, uint16_t x, uint16_t y, uint layer, uint16_t id);
    void drawFeature(uint16_t x, uint16_t y, uint layer, uint16_t id);
    void drawAllNetsAsInactive(QString source, QString dest);
    void redrawNetsColorize(QString source, QString dest);
    void experimental_1();              // 3D fill layer map with vss and vcc
    void experimental_2();              // Save layer map to file
    void experimental_3();              // Creates transistors paths hinted by transdef bounding boxes
    void experimental_4();              // Creates transistors paths based on our feature bitmap
    bool scanForTransistor(uchar const *p, QRect t, uint &x, uint &y);
    void edgeWalk(uchar const *p, QPainterPath &path, uint x, uint y);
    uint edgeWalkFindDir(uchar const *p, uint x, uint y, uint startDir);
    bool scanForTransistor_4(uchar const *p, uint &offset);
};

#endif // CLASSCHIP_H
