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
    { "metal_VCC_GND" },
    { "vias_VCC_GND" },
};

ClassChip::ClassChip() :
    m_last_image(0)
{
}

ClassChip::~ClassChip()
{
}

QImage &ClassChip::getImage(uint i)
{
    if (i < uint(m_img.count()))
    {
        m_last_image = i;
        return m_img[i];
    }
    return getLastImage();
}

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
        QString png_file = dir + "/Z80_" + image + ".png";
        qInfo() << "Loading " + png_file;
        e.processEvents(QEventLoop::AllEvents); // Don't freeze the GUI
        if (img.load(png_file))
        {
            int w = img.width();
            int h = img.height();
            int d = img.depth();
            QImage::Format f = img.format();
            qInfo() << "Image w=" << w << " h=" << h << " depth=" << d << "format=" << f;
            img.setText("name", image); // Set the key with the layer/image name
            m_img.append(img);
        }
        else
        {
            qWarning() << "Error loading " + image;
            return false;
        }
    }
    qInfo() << "Loaded " << m_img.count() << " images";
    return true;
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
        qInfo() << "Processing image " << image;
        e.processEvents(QEventLoop::AllEvents); // Don't freeze the GUI
        QImage new_image = image.convertToFormat(QImage::Format_Grayscale8, Qt::AutoColor);
        new_image.setText("name", "bw." + image.text("name"));
        new_images.append(new_image);
    }
    m_img.append(new_images);
    return true;
}

/*
 * Load nodenames.js
 */
bool ClassChip::loadNodenames(QString dir)
{
    QString nodenames_file = dir + "/nodenames.js";
    qInfo() << "Loading " << nodenames_file;
    QFile file(nodenames_file);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        m_nodenames.clear();
        while(!in.atEnd())
        {
            line = in.readLine();
            if (!line.startsWith('/') && line.indexOf(':') != -1)
            {
                line.chop(1);
                list = line.split(':');
                if (list.length()==2)
                {
                    int key = list[1].toInt();
                    if (!m_nodenames.contains(key))
                        m_nodenames[key] = list[0];
                    else
                        qWarning() << "Duplicate key " << key;
                }
                else
                    qWarning() << "Invalid line " << list;
            }
            else
                qDebug() << "Skipping " << line;
        }
        file.close();
        qInfo() << "Loaded " << m_nodenames.count() << " names";
        return true;
    }
    else
        qWarning() << "Error opening nodenames.js";
    return false;
}

/*
 * Loads transdefs.js
 */
bool ClassChip::loadTransdefs(QString dir)
{
    QString transdefs_file = dir + "/transdefs.js";
    qInfo() << "Loading " << transdefs_file;
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
                    // The order of values in the data file is: rl, rr, tl, tb
                    t.box = QRect(QPoint(list[4].toInt(), y - list[6].toInt()), QPoint(list[5].toInt(), y - list[7].toInt()));
                    t.area = list[12].toUInt();
                    t.is_weak = list[13] == "true";

                    m_transdefs.append(t);
                }
                else
                    qWarning() << "Invalid line " << list;
            }
            else
                qDebug() << "Skipping " << line;
        }
        file.close();
        qInfo() << "Loaded " << m_transdefs.count() << " transistor definitions";
        return true;
    }
    else
        qWarning() << "Error opening transdefs.js";
    return false;
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

/*
 * Attempts to load all chip resource that we expect to have
 */
bool ClassChip::loadChipResources(QString dir)
{
    qInfo() << "Loading chip resources from " << dir;
    if (loadImages(dir) && loadNodenames(dir) && loadTransdefs(dir) && addTransistorsLayer() && convertToGrayscale())
    {
        buildLayerMap();

        // XXX not coded, just experimental
        bool ok;
        QVector<xy> trans_outlines = getOutline(getImageByName("bw.layermap", ok), TRANSISTOR);

        m_dir = dir;
        qInfo() << "Completed loading chip resources";
        emit refresh();
        return true;
    }
    else
        qWarning() << "Loading chip resource failed";
    return false;
}

void ClassChip::onBuild()
{
    QEventLoop e; // Don't freeze the GUI
    QImage &img = getLastImage();

    QString segdefs_file = m_dir + "/segdefs.js";
    QFile file(segdefs_file);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        qDebug() << "Error loading segdefs.js";
        return;
    }
    QTextStream in(&file);
    QString line;
    QStringList list;
    QVector<QPoint> path;
    m_segdefs.clear();
    int count = 0;
    while(!in.atEnd())
//    while(!in.atEnd() && count<1732)
//    while(!in.atEnd() && count<100)
    {
        if ((rand() & 255) > 255)
            continue;
        line = in.readLine();
        if (line.startsWith('['))
//        if (line.startsWith("#[")) // XXX Helps out reading individual lines of the data file
        {
            line.replace("-0.5", "0"); // XXX The data file contains a couple of these coordinates!
            line.replace(".5", ""); // XXX The data file contains qreal values with this fraction but we read ints!
            line = line.mid(2, line.length() - 4);
            list = line.split(',');
            if (list.length() > 4)
            {
                QPainter painter(&img);

                segdef s;
                s.nodenum = list[0].toUInt();
                s.pullup = list[1] == "'+'";
                s.layer = list[2].toUInt();
                if (s.layer == 0)
                    painter.setPen(QColor(0,255,0));
                else if (s.layer == 3)
                    painter.setPen(QColor(255,0,0));
                else
                    painter.setPen(QColor(4 << s.layer,255,0));

                for (int i=3; i<list.length()-1; i+=2)
                {
                    uint x = list[i].toUInt();
                    uint y = img.height() - list[i+1].toUInt() - 1;
                    s.points.append(QPoint(x,y));
//                  s.poly << QPoint(x,y);

                    //painter.fillRect(x,y,4,4,QColor(255,255,0));
                }
#if 0 // One way to do it is to manually draw a closed loop of lines
                for (int i=0; i < s.points.count() - 1; i++)
                    painter.drawLine(s.points[i], s.points[i+1]);
                painter.drawLine(s.points[0], s.points[s.points.length()-1]);
#endif
                QPainterPath path_xxx;
                s.path.setFillRule(Qt::WindingFill);
                s.path.moveTo(s.points[0].x(),s.points[0].y());
                for (int i=1; i < s.points.count(); i++)
                {
                    s.path.lineTo(s.points[i].x(),s.points[i].y());
                }
                s.path.closeSubpath();

#if 1 // If you set a pen to a solid line, or you set a brush, the path will be filled
                QColor c;
                if (s.nodenum == 1) // GND
                    painter.setBrush(QColor(0,255,0)), c = QColor(50,255,50);
                else if (s.nodenum == 2) // VCC
                    painter.setBrush(QColor(255,0,0)), c = QColor(255,50,50);
                else if (s.nodenum == 2) // CLK
                    painter.setBrush(QColor(255,255,255)), c = QColor(255,255,255);
                else
                    painter.setBrush(QColor(4 << s.layer,255,0)), c = QColor(255,255,50);
                painter.setPen(QPen(QColor(255,255,255), 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
#endif
                painter.setOpacity(0.5);
                painter.translate(-0.5, -0.5); // Adjust for Qt's very precise rendering
                painter.drawPath(s.path);

                m_segdefs.append(s);
            }
            count++;
            if (!(count % 100))
            {
                emit refresh();
                e.processEvents(QEventLoop::AllEvents);
            }
        }
    }
    file.close();
    drawTransistors(img);
    emit refresh();
}

QList<int> ClassChip::getNodesAt(int x, int y)
{
    QList<int> list;
    int i = 0; // tmp to help out finding erroneous lines of data
    for(auto s : m_segdefs)
    {
        if (s.path.contains(QPointF(x, y)))
        {
            list.append(s.nodenum);
            //list.append(i);
        }
        i++;
    }
    return list;
}

const QStringList ClassChip::getTransistorsAt(int x, int y)
{
    QStringList list;
    for(auto s : m_transdefs)
    {
        if (s.box.contains(QPoint(x, y)))
        {
            list.append(s.name);
        }
    }
    return list;
}

const QStringList ClassChip::getNodenamesFromNodes(QList<int> nodes)
{
    QList<QString> list;
    for(auto i : nodes)
    {
        if (m_nodenames.contains(i) && !list.contains(m_nodenames[i])) // Do not create duplicates
            list.append(m_nodenames[i]);
    }
    return list;
}

QImage &ClassChip::getImageByName(QString name, bool &ok)
{
    static QImage img_empty;
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
        if (p_vias[i])
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

/*
 * Returns a vector containing the feature (given by mask) outlines in the map x,y space
 *
 * XXX This is just for experimenting
 */
QVector<xy> &ClassChip::getOutline(QImage &image, uchar mask)
{
    QVector<xy> *outline = new QVector<xy>();
    uint sx = image.width();
    uint sy = image.height();
    const uchar *p_src = image.constBits();
    uchar *p_map = new uchar[sx * sy]{};

    for (uint y=0; y<sy; y++)
    {
        for (uint x=0; x<sx; x++)
        {
            if (p_src[y*sx+x] & mask)
            {
                p_map[y*sx+x] = mask;
            }
        }
    }

    int stride = image.width(); // Create QImage with a specific stride and a cleanup function as needed when using a custom data buffer
    QImage imgmap(p_map, sx, sy, stride, QImage::Format_Grayscale8, [](void *p){ delete[] static_cast<uchar *>(p); }, (void *)p_map);
    imgmap.setText("name", "bw.trans");
    m_img.append(imgmap);

    return *outline;
}

