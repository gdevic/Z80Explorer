#ifndef CLASSCHIP_H
#define CLASSCHIP_H

#include <QImage>
#include <QHash>

// Holds visual definition of a segment (wire with the same voltage level)
struct segdef
{
    uint nodenum;       // Node (segment) number
    QVector<QPainterPath> paths; // Visual QPainter class' list of paths (patches) describing the area
};

struct transdef
{
    QString name;
    uint gatenode;
    uint sourcenode;
    uint drainnode;
    QRect box;
    uint area;
    bool is_weak;
};

struct xy
{
    uint x;
    uint y;
};

/*
 * ClassChip contains functions to hold the chip data
 */
class ClassChip: public QObject
{
    Q_OBJECT

public:
    ClassChip();
    ~ClassChip();

    bool loadChipResources(QString dir); // Attempts to load all expected chip resources
    QImage &getImage(uint i);           // Returns the reference to the image by the image index
    QImage &getLastImage();             // Returns the reference to the last image returned by getImage()
    QList<int> getNodesAt(int x, int y);
    const QStringList getTransistorsAt(int x, int y);
    const QStringList getNodenamesFromNodes(QList<int> nodes);
    const QStringList getLayerNames();  // Returns a list of layer / image names
    const segdef *getSegment(uint nodenum); // Returns the segdef given its node number, nullptr if not found
    const transdef *getTrans(QString name); // Returns transistor definition given its name, nullptr if not found

signals:
    void refresh();                     // One of the images has changed

public slots:
    void onBuild();

private:
    QVector<QImage> m_img;              // Chip layer images
    uint m_last_image;                  // Index of the last image requested by getImage() call
    QString m_dir;                      // Directory containing chip resources (set by loadChipResources)

    QVector<QPolygon> m_poly;
    QHash<uint, segdef> m_segdefs;      // Hash of visual segment definitions, key is the segment node number
    QHash<int, QString> m_nodenames;    // Hash of node names (vcc, vss,...), key is the node number
    QVector<transdef> m_transdefs;      // Array of visual transistor definitions

private:
    bool loadImages(QString dir);       // Loads chip images
    bool loadSegdefs(QString dir);      // Loads segdefs.js
    bool loadNodenames(QString dir);    // Loads nodenames.js
    bool loadTransdefs(QString dir);    // Loads transdefs.js
    bool addTransistorsLayer();         // Inserts an image of the transistors layer
    bool convertToGrayscale();          // Converts loaded images to grayscale format
    void drawTransistors(QImage &img);  // Draws transistors on the given image surface

    QImage &getImageByName(QString name, bool &ok);
    void buildLayerMap();               // Builds a layer map data
    QVector<xy> &getOutline(QImage &image, uchar mask); // Returns a vector containing the feature (mask) outlines
};

#endif // CLASSCHIP_H
