#ifndef CLASSCHIP_H
#define CLASSCHIP_H

#include <QImage>
#include <QHash>

// Contains visual definition of a segment (wire at the same voltage level)
struct segdef
{
    uint nodenum;                // Node (segment) number
    QVector<QPainterPath> paths; // Visual QPainter class' list of paths (patches) describing the area
};

// Contains individual transistor definition
struct transdef
{
    QString name;       // Transistor name (ex. 't251')
    uint gatenode;      // Node (segment) connected to its gate
    uint sourcenode;    // Node (segment) connected to its source
    uint drainnode;     // Node (segment) connected to its drain
    QRect box;          // Visual rectangle where it is (roughly) located
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

    bool loadChipResources(QString dir, bool fullSet);// Attempts to load all expected chip resources
    QImage &getImage(uint i);           // Returns the reference to the image by the image index
    QImage &getImage(QString name, bool &ok); // Returns the reference to the image by the image (embedded) name
    QImage &getLastImage();             // Returns the reference to the last image returned by getImage()
    QList<int> getNodesAt(int x, int y);
    const QStringList getTransistorsAt(int x, int y);
    const QStringList getLayerNames();  // Returns a list of layer / image names
    const segdef *getSegment(uint nodenum); // Returns the segdef given its node number, nullptr if not found
    const transdef *getTrans(QString name); // Returns transistor definition given its name, nullptr if not found

signals:
    void refresh();                     // One of the images has changed

public slots:
    void drawSegdefs();                 // Draws segments and transistors onto the last image (returned by getImage())
    void drawExperimental();            // Experiments...

private:
    QVector<QImage> m_img;              // Chip layer images
    uint m_last_image {};               // Index of the last image requested by getImage() call

    QVector<QPolygon> m_poly;
    QHash<uint, segdef> m_segdefs;      // Hash of segment definitions, key is the segment node number
    QVector<transdef> m_transdefs;      // Array of transistor definitions

    uint m_sx {};                       // X size of all images and maps
    uint m_sy {};                       // Y size of all images and maps
    uint16_t *m_p3[3] {};               // Layers map: [0] diffusion, [1] poly, [2] metal

private:
    bool loadImages(QString dir, bool); // Loads chip images
    bool loadSegdefs(QString dir);      // Loads segdefs.js
    bool loadTransdefs(QString dir);    // Loads transdefs.js
    bool addTransistorsLayer();         // Inserts an image of the transistors layer
    void drawTransistors(QImage &img);  // Draws transistors on the given image surface
    bool convertToGrayscale();          // Converts loaded images to grayscale format
    void buildLayerMap();               // Builds a layer map data
    void shrinkVias(QString name);      // Creates a via layer with 1x1 vias
    void buildLayerImage();             // Builds a layer image only
    // Experimental code
    void fill(const uchar *p_map, uint16_t x, uint16_t y, uint layer, uint16_t id);
    void drawFeature(uint16_t x, uint16_t y, uint layer, uint16_t id);
};

#endif // CLASSCHIP_H
