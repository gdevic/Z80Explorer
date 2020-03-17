#ifndef CLASSCHIP_H
#define CLASSCHIP_H

#include <QImage>

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
    QImage &getImage(ChipLayer l) { return m_img[l]; }
    QImage &getImage(uint set, uint i) { return set ? m_imgbw[i % 7] : m_img[i % 7]; }

signals:
    void refresh();                     // Image has changed

private:
    QImage m_img[7];                    // Chip layer pixmaps
    QImage m_imgbw[7];                  // Grayscale (B/W) versions of the images
};

#endif // CLASSCHIP_H
