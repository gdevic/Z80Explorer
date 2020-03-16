#ifndef CLASSCHIP_H
#define CLASSCHIP_H

#include <QObject>
#include <QImage>

/**
 * ClassChip contains functions to hold the chip data
 */
class ClassChip: public QObject
{
    Q_OBJECT

public:
    ClassChip();
    ~ClassChip();
    bool loadChipResources(QString dir); // Attempts to load all expected chip resources

private:
    enum { Burried, Diffusion, Ions, Metal, Pads, Poly, Vias } ChipLayer;
    QImage m_img[7];
};

#endif // CLASSCHIP_H
