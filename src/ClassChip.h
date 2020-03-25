#ifndef CLASSCHIP_H
#define CLASSCHIP_H

#include <QImage>
#include <QHash>

struct segdef
{
    uint nodenum;
    bool pullup;
    uint layer;
    QVector<QPoint> points; // XXX Do we need both points *and* path?
    QPainterPath path;
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
    const segdef *getSegment(uint nodenum); // Search for the segdef given its node number, nullptr if not found
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
    QVector<segdef> m_segdefs;          // Array of visual segment definitions
    QHash<int, QString> m_nodenames;    // Hash of node numbers to their names (vcc, vss,...)
    QVector<transdef> m_transdefs;      // Array of visual transistor definitions

private:
    bool loadImages(QString dir);       // Loads chip images
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
