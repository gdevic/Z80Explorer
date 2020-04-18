#include "ClassChip.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QPainter>

// List of z80 chip resource images / layers. "Z80_" and ".png" are appended only when loading the files.
static const QStringList files =
{
    { "diffusion" },
    { "polysilicon" },
    { "metal" },
    { "pads" },
    { "buried" }, // If not loading the full set, stop at this one
    { "vias" },
    { "ions" },
    { "metal_vcc_gnd" },
    { "vias_vcc_gnd" },
};

// We can use any bits, but these make the map looking good when simply viewed it as an image
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
 * We can load the full set or only a smaller subset of images
 */
bool ClassChip::loadChipResources(QString dir, bool fullSet)
{
    qInfo() << "Loading chip resources from" << dir;
    if (loadImages(dir, fullSet) && loadSegdefs(dir) && loadTransdefs(dir) && addTransistorsLayer() && convertToGrayscale())
    {
        // Allocate buffers for layers map
        uint sx = m_img[0].width();
        uint sy = m_img[0].height();

        p3[0] = new uint16_t[sy * sx] {};
        p3[1] = new uint16_t[sy * sx] {};
        p3[2] = new uint16_t[sy * sx] {};

        // Build a layer image; either the complete map or just the image if we don't have the full data set
        if (fullSet)
        {
            buildLayerMap();
            shrinkVias("bw.layermap");
        }
        else // Build a simple layer image (not suitable to extract features)
            buildLayerImage();

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
bool ClassChip::loadImages(QString dir, bool fullSet)
{
    QEventLoop e; // Don't freeze the GUI
    QImage img;
    m_img.clear();
    for (auto image : files)
    {
        if (!fullSet && (image == "buried"))
            return true;
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
                    t.box = QRect(QPoint(list[4].toInt(), y - list[7].toInt()), QPoint(list[5].toInt() - 1, y - list[6].toInt() - 1));

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
 * Returns the reference to the image by the image index.
 * Returns last image if the index is outside the stored images count
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
 * Returns the reference to the image by the image (embedded) name
 * ok is set to false if the image cannot be found. It is not modified on success
 */
QImage &ClassChip::getImage(QString name, bool &ok)
{
    static QImage img_empty;
    for (auto &i : m_img)
        if (i.text("name") == name)
            return i;
    ok = false;
    return img_empty;
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
    painter.setBrush(Qt::darkGray);
    painter.setPen(QPen(QColor(), 0, Qt::NoPen)); // No outlines
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
        QPen pen { QColor(Qt::white) };
        pen.setCosmetic(true);
        painter.setPen(pen);
        if (s.nodenum == 1) // GND
            painter.setBrush(QColor(0,255,0));
        else if (s.nodenum == 2) // VCC
            painter.setBrush(QColor(255,0,0));
        else
            painter.setBrush(QColor(128,255,0));

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
    // Get a pointer to the first byte of each image data
    const uchar *p_diff = getImage("bw.diffusion", ok).constBits();
    const uchar *p_poly = getImage("bw.polysilicon", ok).constBits();
    const uchar *p_metl = getImage("bw.metal", ok).constBits();
    const uchar *p_buri = getImage("bw.buried", ok).constBits();
    const uchar *p_vias = getImage("bw.vias", ok).constBits();
    if (!ok)
    {
        qWarning() << "Unable to load bw.* image";
        return;
    }
    // ...and of the destination buffer
    uchar *p_dest = layermap.bits();

    for (uint i=0; i < sy*sx; i++)
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
    m_img.prepend(layermap);
}

/*
 * Shrink each via to only 1x1 representative pixel
 * Assumptions:
 *  1. There are no vias to nowhere: all vias are valid and are connecting two layers
 *  2. Vias are square in shape
 * Given #1, top-left pixel on each via block is chosen as a reprenentative
 * There are features on the vias' images that are not square, but those are not functional vias
 */
void ClassChip::shrinkVias(QString name)
{
    qInfo() << "Shrinking the via map" << name;
    bool ok = true;
    // Shallow copy constructor, will create a new image once data buffer is written to
    QImage img(getImage(name, ok));
    Q_ASSERT(ok);

    uint sx = img.width();
    uint sy = img.height();
    const uchar vias[3] = { BURIED, VIA_DIFF, VIA_POLY };
    uchar *p = img.bits();

    // Trim 1 pixel from each edge
    memset(p, 0, sx);
    memset(p + (sy - 1) * sx, 0, sx);
    for (uint y = 1; y < sy; y++)
        *(uint16_t *)(p + y * sx - 1) = 0;

    for (uint i = 0; i < 3; i++)
    {
        const uchar via = vias[i];
        uint offset = 0;
        while (offset < (sx - 1) * sy)
        {
            if (p[offset] & via)
            {
                // Reduce a square via to a single point
                uint line = offset;
                while(true)
                {
                    uint pix = line;
                    while (p[pix] & via)
                        p[pix] = p[pix] & ~via, pix++;
                    line += sx; // The next pixel below
                    if (! (p[line] & via))
                        break;
                }
                p[offset] |= via; // Bring back the top-leftmost via
            }
            offset++;
        }
    }
    img.setText("name", "bw.layermap2");
    m_img.append(img);
}

/*
 * Builds a layer image only, used for instances were we only loaded a subset of images
 */
void ClassChip::buildLayerImage()
{
    qInfo() << "Building the layer map";
    uint sx = m_img[0].width();
    uint sy = m_img[0].height();
    QImage layermap(sx, sy, QImage::Format_Grayscale8);

    bool ok = true;
    // Get a pointer to the first byte of each image data
    const uchar *p_diff = getImage("bw.diffusion", ok).constBits();
    const uchar *p_poly = getImage("bw.polysilicon", ok).constBits();
    const uchar *p_metl = getImage("bw.metal", ok).constBits();
    if (!ok)
    {
        qWarning() << "Unable to load bw.* image";
        return;
    }
    // ...and of the destination buffer
    uchar *p_dest = layermap.scanLine(0);

    for (uint i=0; i < sy*sx; i++)
    {
        uchar diff = !!p_diff[i] << DIFF_SHIFT;
        uchar poly = !!p_poly[i] << POLY_SHIFT;
        uchar metl = !!p_metl[i] << METAL_SHIFT;

        uchar c = diff | poly | metl;

        // Mark a transistor area (poly over diffusion without a buried contact)
        // Transistor path also splits the diffusion area into two, so we remove DIFF over these traces
        if ((c & (DIFF | POLY | BURIED)) == (DIFF | POLY)) c = (c & ~DIFF) | TRANSISTOR;

        // Enhance bw image ... Not an exact science, experiment...
        (c & METAL) ? c |= 0x0F : 0; // Metal layer is the faintest, lift it up a bit
        (c & TRANSISTOR) ? c |= 0xF0 : 0; // Transistors should be the most obvious features
        c <<= 1; // Make everything a little bit brighter

        p_dest[i] = c;
    }
    layermap.setText("name", "bw.layermap");
    m_img.prepend(layermap);
}

/******************************************************************************
 * Experimental code
 ******************************************************************************/

struct xy
{
    uint16_t x;
    uint16_t y;
};

struct xyl
{
    uint16_t x;
    uint16_t y;
    uint layer;
};

/*
 * Creates 3 layers' fill of data surfaces based on the map data
 */
void ClassChip::fill(uint16_t *p3[3], uint sx, const uchar *p_map, uint16_t x, uint16_t y, uint layer, uint16_t id)
{
    const uchar layerMasks[3] = { DIFF, POLY, METAL };

    QVector<xyl> listLayers;
    listLayers.append(xyl {x,y,layer});

    while (!listLayers.isEmpty())
    {
        xyl posl = listLayers.at(listLayers.count() - 1);
        listLayers.removeLast();
        const uchar layerMask = layerMasks[posl.layer];

        QVector<xy> listSeed;
        listSeed.append(xy {posl.x, posl.y});

        while (!listSeed.isEmpty())
        {
            xy pos = listSeed.at(listSeed.count() - 1);
            listSeed.removeLast();
            uint offset = pos.x + pos.y * sx;
            uchar c = p_map[offset];
            if ((c & layerMask) && !p3[posl.layer][offset])
            {
                p3[posl.layer][offset] = id;

                listSeed.append(xy {pos.x, uint16_t(pos.y - 1)});
                listSeed.append(xy {pos.x, uint16_t(pos.y + 1)});
                listSeed.append(xy {uint16_t(pos.x - 1), pos.y});
                listSeed.append(xy {uint16_t(pos.x + 1), pos.y});

                // Do we have a via to jump to another layer?
                if (c & (BURIED | VIA_DIFF | VIA_POLY))
                {
                    uchar c2 = c & ~layerMask;
                    uint layer = 3;
                    if (posl.layer == 0)
                    {
                        if (c2 & BURIED) layer = 1;
                        else if (c2 & VIA_DIFF) layer = 2;
                        else Q_ASSERT(0);
                    }
                    else if (posl.layer == 1)
                    {
                        if (c2 & BURIED) layer = 0;
                        else if (c2 & VIA_POLY) layer = 2;
                        else Q_ASSERT(0);
                    }
                    else if (posl.layer == 2)
                    {
                        if (c2 & VIA_DIFF) layer = 0;
                        else if (c2 & VIA_POLY) layer = 1;
                        else continue; // ok, buried via is out of reach for metal layer
                    }
                    else Q_ASSERT(0);
                    Q_ASSERT(layer < 3);

                    if (p3[layer])
                        listLayers.append(xyl {pos.x, pos.y, layer});
                }
            }
        }
    }
}

void ClassChip::drawFeature(QString name, uint16_t x, uint16_t y, uint layer, uint16_t id)
{
    uint sx = m_img[0].width();
    uint sy = m_img[0].height();

    bool ok = true;
    // Get a pointer to the first byte of the layer map data
    const uchar *p_map = getImage("bw.layermap2", ok).constBits();
    Q_ASSERT(ok);

    QElapsedTimer timer;
    timer.start();

    fill(p3, sx, p_map, x, y, layer, id);

    qDebug() << "Feature build operation took" << timer.elapsed() << "milliseconds";

    // Out of 3 layers, compose one visual image that we'd like to see
    QImage imgFinal(sx, sy, QImage::Format_Grayscale8);
    uchar *p_final = imgFinal.bits();
    for (uint i = 0; i < sx * sy; i++)
        p_final[i] = (p3[0][i] | p3[1][i] | p3[2][i]) ? 0xFF : 0;

    imgFinal.setText("name", name);
    m_img.append(imgFinal);
    qDebug() << "Created image map" << name;
}

/*
 * Run with the command "ex(1)"
 */
void ClassChip::drawExperimental()
{
    qDebug() << "Experimental drawing";

    drawFeature("bw.vss", 100,100, 2, 1); // vss
    drawFeature("bw.vcc", 4456,2512, 2, 2); // vcc
    drawFeature("bw.clk", 4476,4769, 2, 3); // clk
}
