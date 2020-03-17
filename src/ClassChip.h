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
    QImage &getImage(ChipLayer l) { return m_img[l]; }

signals:
    void refresh();                     // Image has changed

private:
    QImage m_img[7];                    // Chip layer pixmaps
    QImage m_imgbw[7];                  // Grayscale (B/W) versions of the images
};

#endif // CLASSCHIP_H
