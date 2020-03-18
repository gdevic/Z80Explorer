#ifndef CLASSCHIP_H
#define CLASSCHIP_H

#include <QImage>
#include <QHash>

struct segdef
{
    uint nodenum;
    bool pullup;
    uint layer;
    QVector<QPoint> points;
    QPolygon poly;
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

    enum ChipLayer { Burried, Diffusion, Ions, Metal, Pads, Poly, Vias };
    Q_ENUM(ChipLayer)
    QImage &getImage(uint i);           // Returns the reference to the image by the image index
    QImage &getLastImage();             // Returns the reference to the last image returned by getImage()
    QList<int> getNodesAt(int x, int y);
    QList<QString> getTransistorsAt(int x, int y);
    QList<QString> getNodenamesFromNodes(QList<int> nodes);

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
    bool convertToGrayscale();          // Converts loaded images to grayscale format
    bool loadNodenames(QString dir);    // Loads nodenames.js
    bool loadTransdefs(QString dir);    // Loads transdefs.js
};

#endif // CLASSCHIP_H
