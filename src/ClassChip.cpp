#include "ClassChip.h"
#include <QImage>
#include <QDebug>
#include <QDir>
#include <QEventLoop>

ClassChip::ClassChip()
{
}

ClassChip::~ClassChip()
{
}

/**
 * Attempts to load all expected chip resources
 */
bool ClassChip::loadChipResources(QString dir)
{
    qDebug() << "Loading chip resources from " << dir;

    static QString files[] = {"Z80_buried.png", "Z80_diffusion.png", "Z80_ions.png", "Z80_metal.png", "Z80_pads.png", "Z80_polysilicon.png", "Z80_vias.png"};

    QEventLoop e; // Don't freeze the GUI

    for (int i = 0; i < 7; i++)
    {
        QString png_file = dir + "/" + files[i];
        qDebug() << "Loading " + png_file;
        e.processEvents(QEventLoop::AllEvents);
        if (m_img[i].load(png_file))
        {
            int w = m_img[i].width();
            int h = m_img[i].height();
            int d = m_img[i].depth();
            QImage::Format f = m_img[i].format();
            qDebug() << w << h << d << f;
        }
        else
        {
            qDebug() << "Error loading " + files[i];
            return false;
        }
    }
    qDebug() << "Completed resource loading";

    qDebug() << "Converting to 8bpp format...";
    for (int i = 0; i < 7; i++)
    {
        qDebug() << "Processing " << files[i];
        e.processEvents(QEventLoop::AllEvents);
        m_img[i] = m_img[i].convertToFormat(QImage::Format_Grayscale8, Qt::AutoColor);
    }
    qDebug() << "Done processing images";

    return true;
}
