#include "ClassChip.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QPainter>

// List of z80 chip resource images / layers. "Z80_" and ".png" are appended only when loading the files.
static const QStringList files =
{
    { "diffusion" },
    { "polysilicon" },
    { "metal" },
    { "buried" },
    { "vias" },
    { "ions" },
    { "pads" },
    { "metal_vcc_gnd" },
    { "vias_vcc_gnd" },
};

/*
 * Attempts to load all chip resource that we expect to have
 */
bool ClassChip::loadChipResources(QString dir)
{
    qInfo() << "Loading chip resources from" << dir;
    if (loadImages(dir) && loadSegdefs(dir) && loadTransdefs(dir) && addTransistorsLayer() && convertToGrayscale())
    {
        buildLayerMap(); // XXX

        qInfo() << "Completed loading chip resources";
        emit refresh();
        return true;
    }
    else
        qWarning() << "Loading chip resource failed";
    return false;
}

/*
 * Load chip images
 */
bool ClassChip::loadImages(QString dir)
{
    QEventLoop e; // Don't freeze the GUI
    QImage img;
    m_img.clear();
    for (auto image : files)
    {
        QString png_file = dir + "/z80_" + image + ".png";
        qInfo() << "Loading" + png_file;
        e.processEvents(QEventLoop::AllEvents); // Don't freeze the GUI
        if (img.load(png_file))
        {
            int w = img.width();
            int h = img.height();
            int d = img.depth();
            QImage::Format f = img.format();
            qInfo() << "Image w=" << w << "h=" << h << "depth=" << d << "format=" << f;
            img.setText("name", image); // Set the key with the layer/image name
            m_img.append(img);
        }
        else
        {
            qWarning() << "Error loading" + image;
            return false;
        }
    }
    qInfo() << "Loaded" << m_img.count() << "images";
    return true;
}

/*
 * Loads segdefs.js
 */
bool ClassChip::loadSegdefs(QString dir)
{
    QString segdefs_file = dir + "/segdefs.js";
    qInfo() << "Loading" << segdefs_file;
    QFile file(segdefs_file);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        m_segdefs.clear();
        while(!in.atEnd())
        {
            line = in.readLine();
            if (line.startsWith('['))
            {
                line.replace("-0.5", "0"); // segdefs.js contains a couple of negative coordinates
                line.replace(".5", ""); // segdefs.js contains qreal values with (this) fraction but we want ints
                line = line.mid(2, line.length() - 4);
                list = line.split(',');
                if (list.length() > 4)
                {
                    segdef s;
                    uint key = list[0].toUInt();
                    QPainterPath path;
                    for (int i=3; i<list.length()-1; i+=2)
                    {
                        uint x = list[i].toUInt();
                        uint y = m_img[0].height() - list[i+1].toUInt() - 1;
                        if (i == 3)
                            path.moveTo(x,y);
                        else
                            path.lineTo(x,y);
                    }
                    path.closeSubpath();

                    if (!m_segdefs.contains(key))
                    {
                        s.nodenum = key;
                        s.paths.append(path);
                        m_segdefs[key] = s;
                    }
                    else
                        m_segdefs[key].paths.append(path);
                }
                else
                    qWarning() << "Invalid line" << line;
            }
            else
                qDebug() << "Skipping" << line;
        }
        file.close();
        qInfo() << "Loaded" << m_segdefs.count() << "segdefs";
        return true;
    }
    else
        qWarning() << "Error opening segdefs.js";
    return false;
}

/*
 * Loads transdefs.js
 */
bool ClassChip::loadTransdefs(QString dir)
{
    QString transdefs_file = dir + "/transdefs.js";
    qInfo() << "Loading" << transdefs_file;
    QFile file(transdefs_file);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        m_transdefs.clear();
        uint y = m_img[0].height() - 1;
        while(!in.atEnd())
        {
            line = in.readLine();
            if (line.startsWith('['))
            {
                line.replace('[', ' ').replace(']', ' '); // Make it a simple list of numbers
                line.chop(2);
                list = line.split(',', QString::SkipEmptyParts);
                if (list.length()==14 && list[0].length() > 2)
                {
                    transdef t;
                    t.name = list[0].mid(2, list[0].length()-3);
                    t.gatenode = list[1].toUInt();
                    t.sourcenode = list[2].toUInt();
                    t.drainnode = list[3].toUInt();
                    // The order of values in the data file is: [4,5,6,7] => left, right, bottom, top
                    // The Y coordinates in the input data stream are inverted, with 0 starting at the bottom
                    t.box = QRect(QPoint(list[4].toInt(), y - list[7].toInt()), QPoint(list[5].toInt(), y - list[6].toInt()));

                    m_transdefs.append(t);
                }
                else
                    qWarning() << "Invalid line" << list;
            }
            else
                qDebug() << "Skipping" << line;
        }
        file.close();
        qInfo() << "Loaded" << m_transdefs.count() << "transistor definitions";
        return true;
    }
    else
        qWarning() << "Error opening transdefs.js";
    return false;
}

/*
 * Returns the reference to the image by the image index
 */
QImage &ClassChip::getImage(uint i)
{
    if (i < uint(m_img.count()))
    {
        m_last_image = i;
        return m_img[i];
    }
    return getLastImage();
}

/*
 * Returns the reference to the last image returned by getImage()
 */
QImage &ClassChip::getLastImage()
{
    static QImage img_empty;
    if (m_last_image < uint(m_img.count()))
        return m_img[m_last_image];
    return img_empty;
}

/*
 * Returns a list of layer / image names, text stored with each image
 */
const QStringList ClassChip::getLayerNames()
{
    QStringList names;
    for (auto name : m_img)
        names.append(name.text("name"));
    return names;
}

QList<int> ClassChip::getNodesAt(int x, int y)
{
    QList<int> list;
    for(auto s : m_segdefs)
    {
        for (auto path : s.paths)
        {
            if (path.contains(QPointF(x, y)))
                list.append(s.nodenum);
        }
    }
    return list;
}

const QStringList ClassChip::getTransistorsAt(int x, int y)
{
    QStringList list;
    for(auto s : m_transdefs)
    {
        if (s.box.contains(QPoint(x, y)))
            list.append(s.name);
    }
    return list;
}

/*
 * Returns the segdef given its node number, nullptr if not found
 */
const segdef *ClassChip::getSegment(uint nodenum)
{
    if (m_segdefs.contains(nodenum))
        return &m_segdefs[nodenum];
    return nullptr;
}

/*
 * Returns transistor definition given its name, nullptr if not found
 */
const transdef *ClassChip::getTrans(QString name)
{
    for (int i=0; i<m_transdefs.size(); i++)
    {
        if (m_transdefs.at(i).name == name)
            return &m_transdefs.at(i);
    }
    return nullptr;
}

/*
 * Inserts an image of the transistors layer
 */
bool ClassChip::addTransistorsLayer()
{
    QImage trans(m_img[0].width(), m_img[0].height(), QImage::Format_ARGB32);
    drawTransistors(trans);
    trans.setText("name", "transistors");
    m_img.append(trans);
    return true;
}

/*
 * Draws transistors on the given image surface
 */
void ClassChip::drawTransistors(QImage &img)
{
    QPainter painter(&img);
    painter.setBrush(QColor(255,255,255));
    painter.setPen(QPen(QColor(255,0,255), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter.setOpacity(0.5);
    painter.translate(-0.5, -0.5); // Adjust for Qt's very precise rendering
    for (auto s : m_transdefs)
        painter.drawRect(s.box);
}

/*
 * Converts loaded images to grayscale format
 */
bool ClassChip::convertToGrayscale()
{
    qInfo() << "Converting images to grayscale format...";
    QEventLoop e; // Don't freeze the GUI
    QVector<QImage> new_images;
    for (auto image : m_img)
    {
        qInfo() << "Processing image" << image;
        e.processEvents(QEventLoop::AllEvents); // Don't freeze the GUI
        QImage new_image = image.convertToFormat(QImage::Format_Grayscale8, Qt::AutoColor);
        new_image.setText("name", "bw." + image.text("name"));
        new_images.append(new_image);
    }
    m_img.append(new_images);
    return true;
}

void ClassChip::drawSegdefs()
{
    QEventLoop e; // Don't freeze the GUI
    QImage &img = getLastImage();

    int count = 0;
    for (auto s : m_segdefs)
    {
        QPainter painter(&img);

        QColor c;
        if (s.nodenum == 1) // GND
            painter.setBrush(QColor(0,255,0)), c = QColor(50,255,50);
        else if (s.nodenum == 2) // VCC
            painter.setBrush(QColor(255,0,0)), c = QColor(255,50,50);
        else if (s.nodenum == 2) // CLK
            painter.setBrush(QColor(255,255,255)), c = QColor(255,255,255);
        else
            painter.setBrush(QColor(128,255,0)), c = QColor(255,255,50);
        painter.setPen(QPen(QColor(255,255,255), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));

        painter.setOpacity(0.5);
        painter.translate(-0.5, -0.5); // Adjust for Qt's very precise rendering

        for (auto path : s.paths)
            painter.drawPath(path);

        count++;
        if (!(count % 400))
        {
            emit refresh();
            e.processEvents(QEventLoop::AllEvents);
        }
    }
    drawTransistors(img);
    emit refresh();
    qDebug() << "Finished drawing segdefs";
}

// Set any bits, but these provide a compelling layer image when viewed:
#define DIFF_SHIFT       6
#define POLY_SHIFT       5
#define METAL_SHIFT      4
#define BURIED_SHIFT     3
#define VIA_DIFF_SHIFT   2
#define VIA_POLY_SHIFT   1
#define TRANSISTOR_SHIFT 7

#define DIFF       (1 << DIFF_SHIFT)
#define POLY       (1 << POLY_SHIFT)
#define METAL      (1 << METAL_SHIFT)
#define BURIED     (1 << BURIED_SHIFT)
#define VIA_DIFF   (1 << VIA_DIFF_SHIFT)
#define VIA_POLY   (1 << VIA_POLY_SHIFT)
#define TRANSISTOR (1 << TRANSISTOR_SHIFT)

QImage &ClassChip::getImageByName(QString name, bool &ok)
{
    static QImage img_empty;
    ok = true;
    for (int i=0; i<m_img.count(); i++)
    {
        if (m_img[i].text("name")==name)
            return m_img[i];
    }
    ok = false;
    return img_empty;
}

/*
 * Builds a layer map data
 */
void ClassChip::buildLayerMap()
{
    qInfo() << "Building the layer map";
    uint sx = m_img[0].width();
    uint sy = m_img[0].height();
    QImage layermap(sx, sy, QImage::Format_Grayscale8);

    bool ok = true;
    QImage img_diff(getImageByName("bw.diffusion", ok));
    QImage img_poly(getImageByName("bw.polysilicon", ok));
    QImage img_metl(getImageByName("bw.metal", ok));
    QImage img_buri(getImageByName("bw.buried", ok));
    QImage img_vias(getImageByName("bw.vias", ok));
    //QImage img_ions(getImageByName("bw.ions", ok));
    if (!ok)
    {
        qWarning() << "Unable to load bw.* image";
        return;
    }

    // Get the pointer to the first byte in each image data
    uchar *p_diff = img_diff.bits();
    uchar *p_poly = img_poly.bits();
    uchar *p_metl = img_metl.bits();
    uchar *p_buri = img_buri.bits();
    uchar *p_vias = img_vias.bits();
    //uchar *p_ions = img_ions.bits();
    // ...and of the destination buffer
    uchar *p_dest = layermap.bits();

    for (uint i=0; i<sy*sx; i++)
    {
        uchar diff = !!p_diff[i] << DIFF_SHIFT;
        uchar poly = !!p_poly[i] << POLY_SHIFT;
        uchar metl = !!p_metl[i] << METAL_SHIFT;
        uchar buri = !!p_buri[i] << BURIED_SHIFT;
        uchar viad = !!(p_vias[i] && diff) << VIA_DIFF_SHIFT;
        uchar viap = !!(p_vias[i] && poly) << VIA_POLY_SHIFT;
        //uchar ions = (!!(*p_ions++)) << VIA_DIFF_SHIFT;  XXX how to process ions?

        // Vias are connections from metal to (poly or diffusion) layer
        if (false && p_vias[i]) // XXX I am tired of watching this warning all the time...
        {
            // Check that the vias are actually connected to either poly or metal
            if (!diff && !poly)
                qDebug() << "Via to nowhere at:" << i%sx << i/sx;
            else if (!metl)
                qDebug() << "Via without metal at:" << i%sx << i/sx;
            else if (metl && poly && diff)
                qDebug() << "Via both to polysilicon and diffusion at:" << i%sx << i/sx;
            else if (buri)
                qDebug() << "Buried under via at:" << i%sx << i/sx;
        }

        uchar c = diff | poly | metl | buri | viad | viap;

        // Check valid combinations of layers and correct them
        if (1)
        {
            // These combinations appear in the Z80 layers, many are valid but some are non-functional:
            switch (c)
            {
            case (0                                       ): c = 0                                       ; break; // - No features
            case (DIFF                                    ): c = DIFF                                    ; break; // - Diffusion area
            case (     POLY                               ): c =      POLY                               ; break; // - Poly trace
            case (DIFF|POLY                               ): c = DIFF|POLY                               ; break; // - *Transistor*
            case (          METAL                         ): c =           METAL                         ; break; // - Metal trace
            case (DIFF|     METAL                         ): c = DIFF|     METAL                         ; break; // - Diffusion area with a metal trace on top
            case (     POLY|METAL                         ): c =      POLY|METAL                         ; break; // - Poly and metal traces
            case (DIFF|POLY|METAL                         ): c = DIFF|POLY|METAL                         ; break; // - *Transistor* with a metal trace running over it
            case (                BURIED                  ): c =                 0                       ; break; // X (invalid buried connection) -> ignore BURIED
            case (DIFF|           BURIED                  ): c = DIFF|           0                       ; break; // X (invalid buried connection, no poly) -> ignore BURIED
            case (     POLY|      BURIED                  ): c =      POLY|      0                       ; break; // X (invalid buried connection, no diffusion) -> ignore BURIED
            case (DIFF|POLY|      BURIED                  ): c = DIFF|POLY|      BURIED                  ; break; // - Poly connected to diffusion
            case (          METAL|BURIED                  ): c =           METAL|0                       ; break; // X (invalid buried connection, no poly) -> ignore BURIED
            case (DIFF|     METAL|BURIED                  ): c = DIFF|     METAL|0                       ; break; // X (invalid buried connection, no poly) -> ignore BURIED
            case (     POLY|METAL|BURIED                  ): c =      POLY|METAL|0                       ; break; // X (invalid buried connection, no diffusion) -> ignore BURIED
            case (DIFF|POLY|METAL|BURIED                  ): c = DIFF|POLY|METAL|BURIED                  ; break; // - Poly connected to diffusion with a metal trace on top
            case (DIFF|                  VIA_DIFF         ): c = DIFF|                  0                ; break; // X (via to nowhere) -> ignore VIA_DIFF
            case (DIFF|     METAL|       VIA_DIFF         ): c = DIFF|     METAL|       VIA_DIFF         ; break; // - Metal connected to diffusion
            case (     POLY|METAL|                VIA_POLY): c =      POLY|METAL|                VIA_POLY; break; // - Metal connected to poly
            default:
                qWarning() << "Unexpected layer combination:" << c;
            }
            // Reassign bits based on the correction
            viad = c & VIA_DIFF;
            viap = c & VIA_POLY;
        }
        // Mark a transistor area (poly over diffusion without a buried contact)
        // Transistor path also splits the diffusion area into two, so we remove DIFF over these traces
        if ((c & (DIFF | POLY | BURIED)) == (DIFF | POLY)) c = (c & ~DIFF) | TRANSISTOR;

        p_dest[i] = c;
    }
    layermap.setText("name", "bw.layermap");
    m_img.append(layermap);
}
