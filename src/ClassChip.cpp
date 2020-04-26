#include "ClassChip.h"
#include "ClassController.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QPainter>
#include <QSettings>

// List of z80 chip resource images / layers. "Z80_" and ".png" are appended only when loading the files.
static const QStringList files =
{
    { "diffusion" },
    { "polysilicon" },
    { "metal" },
    { "buried" },
    { "vias" },
    { "ions" },
};

// --- Feature map bits ---
// We can use any bits, but these make the map looking good when simply viewed it as an image
#define DIFF_SHIFT       6
#define POLY_SHIFT       5
#define METAL_SHIFT      4
#define BURIED_SHIFT     3
#define VIA_DIFF_SHIFT   2
#define VIA_POLY_SHIFT   1
#define IONS_SHIFT       0
#define TRANSISTOR_SHIFT 7

#define IONS       (1 << IONS_SHIFT)
#define DIFF       (1 << DIFF_SHIFT)
#define POLY       (1 << POLY_SHIFT)
#define METAL      (1 << METAL_SHIFT)
#define BURIED     (1 << BURIED_SHIFT)
#define VIA_DIFF   (1 << VIA_DIFF_SHIFT)
#define VIA_POLY   (1 << VIA_POLY_SHIFT)
#define TRANSISTOR (1 << TRANSISTOR_SHIFT)

/*
 * Attempts to load and generate all chip resource that we expect to have
 */
bool ClassChip::loadChipResources(QString dir)
{
    qInfo() << "Loading chip resources from" << dir;
    // Step 1: Load images and resources sourced from the Visual 6502 project
    if (loadImages(dir) && loadSegdefs(dir) && loadTransdefs(dir) && addTransistorsLayer() && convertToGrayscale())
    {
        // Step 2: Generate internal maps and/or load previously generated maps (to speed up the startup time)

        buildFeatureMap(); // Builds the feature map from individual layer images of a die
        shrinkVias("bw.featuremap", "bw.featuremap2");

        // Allocate buffers for, and load the layer map
        m_p3[0] = new uint16_t[m_mapsize] {};
        m_p3[1] = new uint16_t[m_mapsize] {};
        m_p3[2] = new uint16_t[m_mapsize] {};

        // The layer map is large (chip map size X * Y * 2 bytes) times 3 layers
        // If we cannot load the layer map, we need to create it (and then save it)
        if (!loadLayerMap(dir))
        {
            experimental_1(); // Generates a layer map
            experimental_2(); // Saves layer map to file to be loaded next time
        }
        createLayerMapImage("vss.vcc");
        drawAllNetsAsInactive("vss.vcc", "vss.vcc.nets");
        redrawNetsColorize("vss.vcc", "vss.vcc.nets.col");
        connect(&::controller, &ClassController::netNameChanged, this, [this]() // May need different colors for renamed nets
            { redrawNetsColorize("vss.vcc", "vss.vcc.nets.col"); } );
        experimental_4(); // Create transistor paths

        setFirstImage("vss.vcc.nets.col");
        setFirstImage("vss.vcc.nets");

        annotate.init();
        annotate.load(dir); // Load custom annotations

        qInfo() << "Completed loading chip resources";
        return true;
    }
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
    m_sx = m_img[0].width();
    m_sy = m_img[0].height();
    m_mapsize = m_sx * m_sy;

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
                    net_t key = list[0].toUInt();
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
 * Returns the very first image if the index is outside the stored images count
 */
QImage &ClassChip::getImage(uint i)
{
    if (i < uint(m_img.count()))
        return m_img[i];
    return m_img[0];
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
 * Sets the given image to be the first one in m_img vector
 */
void ClassChip::setFirstImage(QString name)
{
    for (int i=1; i < m_img.count(); i++)
    {
        if (m_img[i].text("name") == name)
        {
            m_img.move(i, 0);
            return;
        }
    }
}

/*
 * Returns a list of layer / image names, text stored with each image
 */
const QStringList ClassChip::getLayerNames()
{
    QStringList names;
    for (const auto &name : m_img)
        names.append(name.text("name"));
    return names;
}

/*
 * Returns a list of (unique) nets located at the specified image coordinates
 */
template<bool includeVssVcc>
const QVector<net_t> ClassChip::getNetsAt(int x, int y)
{
    QVector<net_t> list;
    // Use our layer map to read vss, vcc since they are the largest, already mapped, areas
    uint offset = x + y * m_sx;
    if (includeVssVcc)
    {
        if ((m_p3[0][offset] | m_p3[1][offset] | m_p3[2][offset]) == 1) list.append(1); // vss
        if ((m_p3[0][offset] | m_p3[1][offset] | m_p3[2][offset]) == 2) list.append(2); // vcc
    }
    for (const auto &s : m_segdefs)
    {
        if (s.nodenum > 2) // Skip vss and vcc segments
        {
            for (const auto &path : s.paths)
            {
                if (path.contains(QPointF(x, y)) && !list.contains(s.nodenum))
                    list.append(s.nodenum);
            }
        }
    }
    return list;
}

// Explicit instantiation so we don't have to keep the templated code in a header file
template const QVector<net_t> ClassChip::getNetsAt<true>(int, int);
template const QVector<net_t> ClassChip::getNetsAt<false>(int, int);

/*
 * Returns a transistor found at the specified image coordinates or empty string for no transistor
 */
const QString ClassChip::getTransistorAt(int x, int y)
{
    for (const auto &s : m_transdefs)
    {
        if (s.path.contains(QPoint(x, y)))
            return s.name;
    }
    return QString();
}

/*
 * Returns the segdef given its net number, zero if not found
 */
const segdef *ClassChip::getSegment(net_t net)
{
    static const segdef empty;
    if (m_segdefs.contains(net))
        return &m_segdefs[net];
    return &empty;
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
    QImage trans(m_sx, m_sy, QImage::Format_ARGB32);
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
    for (const auto &s : m_transdefs)
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
        qInfo() << "Processing image" << image << image.text("name");
        e.processEvents(QEventLoop::AllEvents); // Don't freeze the GUI
        QImage new_image = image.convertToFormat(QImage::Format_Grayscale8, Qt::AutoColor);
        new_image.setText("name", "bw." + image.text("name"));
        new_images.append(new_image);
    }
    m_img.append(new_images);
    return true;
}

/*
 * Loads layer map
 */
bool ClassChip::loadLayerMap(QString dir)
{
    QString fileName = dir + "/layermap.bin";
    qInfo() << "Loading" << fileName;
    QFile file(fileName);
    if (file.open(QFile::ReadOnly))
    {
        int64_t size = m_mapsize * sizeof(uint16_t); // Size of one layer
        for (uint i = 0; i < 3; i++)
        {
            int64_t read = file.read((char *) m_p3[i], size);
            if (read != size)
            {
                qWarning() << "Error reading" << fileName << "layer" << QString::number(i);
                return false;
            }
        }
        return true;
    }
    qWarning() << "Error opening" << fileName;
    return false;
}

/*
 * Builds the feature map from individual layer images of a die
 */
void ClassChip::buildFeatureMap()
{
    qInfo() << "Building the feature map";
    QImage featuremap(m_sx, m_sy, QImage::Format_Grayscale8);

    bool ok = true;
    // Get a pointer to the first byte of each image data
    const uchar *p_ions = getImage("bw.ions", ok).constBits();
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
    uchar *p_dest = featuremap.bits();

    for (uint i=0; i < m_mapsize; i++)
    {
        uchar ions = !!p_ions[i] << IONS_SHIFT;
        uchar diff = !!p_diff[i] << DIFF_SHIFT;
        uchar poly = !!p_poly[i] << POLY_SHIFT;
        uchar metl = !!p_metl[i] << METAL_SHIFT;
        uchar buri = !!p_buri[i] << BURIED_SHIFT;
        uchar viad = !!(p_vias[i] && diff) << VIA_DIFF_SHIFT;
        uchar viap = !!(p_vias[i] && poly) << VIA_POLY_SHIFT;
        // Ions under a transistor make it non-functional, always closed, disrespective of its gate voltage
        // There are 4 transistors on Z80 die that have this trap and they are already hard-coded in transdefs.js file
        Q_UNUSED(ions);

        // Vias are connections from metal to (poly or diffusion) layer
        if (false && p_vias[i]) // Optionally check vias
        {
            // Check that the vias are actually connected to either poly or metal
            if (!diff && !poly)
                qDebug() << "Via to nowhere at:" << i % m_sx << i / m_sx;
            else if (!metl)
                qDebug() << "Via without metal at:" << i % m_sx << i / m_sx;
            else if (metl && poly && diff)
                qDebug() << "Via both to polysilicon and diffusion at:" << i % m_sx << i / m_sx;
            else if (buri)
                qDebug() << "Buried under via at:" << i % m_sx << i / m_sx;
        }

        uchar c = diff | poly | metl | buri | viad | viap;

        // Check valid combinations of features and correct them
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
                qWarning() << "Unexpected feature combination:" << c;
        }

        // Reassign bits based on the correction
        viad = c & VIA_DIFF;
        viap = c & VIA_POLY;

        // Mark a transistor area (poly over diffusion without a buried contact)
        // Transistor path also splits the diffusion area into two, so we remove DIFF over these traces
        if ((c & (DIFF | POLY | BURIED)) == (DIFF | POLY)) c = (c & ~DIFF) | TRANSISTOR;

        p_dest[i] = c;
    }
    featuremap.setText("name", "bw.featuremap");
    m_img.append(featuremap);
}

/*
 * Shrink each via to only 1x1 representative pixel
 * Assumptions:
 *  1. There are no vias to nowhere: all vias are valid and are connecting two layers
 *  2. Vias are square in shape
 * Given #1, top-left pixel on each via block is chosen as a reprenentative
 * There are features on the vias' images that are not square, but those are not functional vias
 */
void ClassChip::shrinkVias(QString source, QString dest)
{
    qInfo() << "Shrinking the via map" << source << "into" << dest;
    bool ok = true;
    // Shallow copy constructor, will create a new image once data buffer is written to
    QImage img(getImage(source, ok));
    Q_ASSERT(ok);

    const uchar vias[3] = { BURIED, VIA_DIFF, VIA_POLY };
    uchar *p = img.bits();

    // Trim 1 pixel from each edge
    memset(p, 0, m_sx);
    memset(p + (m_sy - 1) * m_sx, 0, m_sx);
    for (uint y = 1; y < m_sy; y++)
        *(uint16_t *)(p + y * m_sx - 1) = 0;

    for (uint i = 0; i < 3; i++)
    {
        const uchar via = vias[i];
        uint offset = 0;
        while (offset < (m_sx - 1) * m_sy)
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
                    line += m_sx; // The next pixel below
                    if (! (p[line] & via))
                        break;
                }
                p[offset] |= via; // Bring back the top-leftmost via
            }
            offset++;
        }
    }
    img.setText("name", dest);
    m_img.append(img);
}

/*
 * Creates a color image from the layer map data
 */
void ClassChip::createLayerMapImage(QString name)
{
    // Out of 3 layers, compose one visual image that we'd like to see
    uint16_t *p = new uint16_t[m_mapsize];

    for (uint i = 0; i < m_mapsize; i++)
    {
        uint16_t net[3] { m_p3[0][i], m_p3[1][i], m_p3[2][i] };
        uint16_t c = 0;
        if ((net[0] == 1) || (net[1] == 1) || (net[2] == 1)) c = ::controller.getColors().get16(1); // vss
        if ((net[0] == 2) || (net[1] == 2) || (net[2] == 2)) c = ::controller.getColors().get16(2); // vcc
        p[i] = c;
    }

    QImage image((uchar *)p, m_sx, m_sy, m_sx * sizeof(int16_t), QImage::Format_RGB16, [](void *p){ delete[] static_cast<int16_t *>(p); }, (void *)p);
    image.setText("name", name);

    m_img.append(image);
    qInfo() << "Created layer map image" << name;
}

/*
 * Draws all nets as inactive into the given image
 */
void ClassChip::drawAllNetsAsInactive(QString source, QString dest)
{
    qInfo() << "Drawing all nets as inactive on top of" << source << "into" << dest;
    bool ok = true;
    // Shallow copy constructor, will create a new image once data buffer is written to
    QImage img(getImage(source, ok));
    Q_ASSERT(ok);

    QPainter painter(&img);
    painter.setPen(QPen(Qt::black, 1, Qt::SolidLine));
    painter.setBrush(QColor(128, 0, 128));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (uint i=3; i<::controller.getSimx().getNetlistCount(); i++)
    {
        for (const auto &path : ::controller.getChip().getSegment(i)->paths)
            painter.drawPath(path);
    }

    img.setText("name", dest);
    m_img.append(img);
}

/*
 * Redraws all nets using the color assigned to each net
 */
void ClassChip::redrawNetsColorize(QString source, QString dest)
{
    qInfo() << "Redrawing all nets/colorize" << source << "into" << dest;
    bool ok = true;
    // Shallow copy constructor, will create a new image once data buffer is written to
    QImage img(getImage(source, ok));
    Q_ASSERT(ok);

    QPainter painter(&img);
    painter.setPen(QPen(Qt::black, 1, Qt::SolidLine));
    painter.setBrush(QColor(128, 0, 128));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (uint i=3; i<::controller.getSimx().getNetlistCount(); i++)
    {
        QString name = ::controller.getNetlist().get(i);
        if (name.startsWith("clk"))
            painter.setBrush(QColor(200, 200, 200));
        else if (name.startsWith("m"))
            painter.setBrush(QColor(128, 192, 128));
        else if (name.startsWith("t"))
            painter.setBrush(QColor(128, 128, 192));
        else if (name.startsWith("pla"))
            painter.setBrush(QColor(128, 192, 192));
        else if (name.startsWith("dbus"))
            painter.setBrush(QColor(0, 192, 0));
        else if (name.startsWith("ubus"))
            painter.setBrush(QColor(128, 255, 0));
        else if (name.startsWith("vbus"))
            painter.setBrush(QColor(0, 255, 128));
        else if (name.startsWith("abus"))
            painter.setBrush(QColor(128, 128, 255));
        else
            painter.setBrush(QColor(128, 0, 128));

        for (const auto &path : ::controller.getChip().getSegment(i)->paths)
            painter.drawPath(path);
    }

    img.setText("name", dest);

    // If the dest image already exists, swap it with the newly drawn one
    for (int i=0; i<m_img.count(); i++)
    {
        if (m_img[i].text("name") == dest)
        {
            m_img[i].swap(img);
            return;
        }
    }
    m_img.append(img);
}

/******************************************************************************
 * Experimental code
 ******************************************************************************/
/*
 * Runs experimental function number n
 */
void ClassChip::experimental(int n)
{
    if (n==1) return experimental_1();
    if (n==2) return experimental_2();
    if (n==3) return experimental_3();
    if (n==4) return experimental_4();
    qWarning() << "Invalid experimental function index" << n;
}

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
 * Fills layer map (this class' m_p3) from the feature bitmap (p_map)
 */
void ClassChip::fill(const uchar *p_map, uint16_t x, uint16_t y, uint layer, uint16_t id)
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
            uint offset = pos.x + pos.y * m_sx;
            uchar c = p_map[offset];
            if ((c & layerMask) && !m_p3[posl.layer][offset])
            {
                m_p3[posl.layer][offset] = id;

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

                    if (m_p3[layer])
                        listLayers.append(xyl {pos.x, pos.y, layer});
                }
            }
        }
    }
}

void ClassChip::drawFeature(uint16_t x, uint16_t y, uint layer, uint16_t id)
{
    bool ok = true;
    // Get a pointer to the first byte of the feature map data
    const uchar *p_map = getImage("bw.featuremap2", ok).constBits();
    Q_ASSERT(ok);

    QElapsedTimer timer;
    timer.start();

    fill(p_map, x, y, layer, id);

    qDebug() << "Feature build operation took" << timer.elapsed() << "milliseconds";
}

/*
 * Run with the command "ex(1)"
 */
void ClassChip::experimental_1()
{
    qInfo() << "Experimental: 3D fill layer map with vss and vcc";

    drawFeature(100,100, 2, 1); // vss
    drawFeature(4456,2512, 2, 2); // vcc
}

/******************************************************************************
 * Experimental code
 ******************************************************************************/

/*
 * Run with the command "ex(2)"
 */
void ClassChip::experimental_2()
{
    qInfo() << "Experimental: save layer map to file";

    QSettings settings;
    QString fileName = settings.value("ResourceDir").toString() + "/layermap.bin";
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly))
    {
        int64_t size = m_mapsize * sizeof(uint16_t); // Size of one layer
        for (uint i = 0; i < 3; i++)
        {
            int64_t written = file.write((const char *) m_p3[i], size);
            if (written != size)
            {
                qWarning() << "Error writing" << fileName << "layer" << QString::number(i);
                return;
            }
        }
        qInfo() << "Layer map saved to file" << fileName;
    }
    else
        qWarning() << "Unable to open" << fileName << "for writing!";
}

/******************************************************************************
 * Experimental code
 ******************************************************************************/

bool ClassChip::scanForTransistor(uchar const *p, QRect t, uint &x, uint &y)
{
    // Find the top-left corner of a transistor within the bounding box
    for (y = t.top(); y < uint(t.bottom() - 1); y++)
    {
        for (x = t.left(); x < uint(t.right() - 1); x++)
            if (p[x + y * m_sx] & TRANSISTOR)
                return true;
    }
    return false;
}

// Directional lookups: starting with the "up", going clockwise in 45 degree steps
static const int dx[8] = { 0, 1, 1, 1, 0,-1,-1,-1 };
static const int dy[8] = {-1,-1, 0, 1, 1, 1, 0,-1 };

#define OFFSET(dx,dy) (x+(dx) + (y+(dy)) * m_sx)
inline uint ClassChip::edgeWalkFindDir(uchar const *p, uint x, uint y, uint startDir)
{
    for (int i = 0; i < 8; i++)
    {
        uint dir = (i + startDir) & 7;
        if (p[ OFFSET(dx[dir], dy[dir]) ] & TRANSISTOR) return dir;
    }
    Q_ASSERT(0);
    return 0;
}
#undef OFFSET

/*
 * Walk the feature (TRANSISTOR) edge and append to the painter path
 */
void ClassChip::edgeWalk(uchar const *p, QPainterPath &path, uint x, uint y)
{
    uint startx = x, starty = y;
    uint nextdir, dir = edgeWalkFindDir(p, x, y, 2); // Initial dir is 2 (->) since we start at its top-leftmost corner
    while (true)
    {
        do
        {
            x += dx[dir];
            y += dy[dir];
            if (Q_UNLIKELY((x == startx) && (y == starty)))
            {
                path.lineTo(x, y); // This is the last point; coincides with the starting point
                return;
            }
            nextdir = edgeWalkFindDir(p, x, y, dir - 3);
        } while (dir == nextdir);
        // Optionally, add dx,dy to make the transistor shape fit better onto the existing segments
        // The segdefs that we use as a background have extra pixel on the right and bottom edges
        uint dx = (nextdir==3 || nextdir==4 || nextdir==5 || nextdir==6);
        uint dy = (nextdir==5 || nextdir==6 || nextdir==7 || nextdir==0);
        path.lineTo(x + dx, y + dy);

        dir = nextdir;
    }
}

/*
 * Run with the command "ex(3)"
 * Creates transistors paths based on our feature bitmap and located at coords taken from each transistor
 * netlist data bounding boxes. The problem with this implementation is that some transistors are meandering
 * close to each other and their bounding boxes overlap. This function is sometimes not able to detect the
 * second overlapping transistor, so it fails to create the outline in a couple of cases.
 */
void ClassChip::experimental_3()
{
    qInfo() << "Experimental: create transistor paths; transistors' locations hinted by transdef";
    QEventLoop e; // Don't freeze the GUI
    int c = 0;

    bool ok = true;
    QImage img(getImage("bw.featuremap", ok));
    Q_ASSERT(ok);

    uint x, y;
    uchar const *p = img.constBits();

    // Known problem: We have a valid transistor bounding box, but few transistors overlap
    for (auto &t : m_transdefs)
    {
        // Find the top-leftmost edge of a transistor
        if (!scanForTransistor(p, t.box, x, y)) // There are few trans in Visual 6502 transdefs.js that are...not (?)
        {
            qWarning() << "Unable to scan transistor" << t.name << t.box;
            continue;
        }
        // Build the path around the transistor
        t.path = QPainterPath(QPointF(x, y));
        edgeWalk(p, t.path, x, y);

        if ((c++ % 100) == 0) e.processEvents(QEventLoop::AllEvents); // Don't freeze the GUI
    }
}

/*
 * Draws all transistors in two shades: yellow for active and gray for inactive
 * with an additional option to highlight all of them irrespective of their state
 */
void ClassChip::expDrawTransistors(QPainter &painter, const QRect &viewport, bool highlightAll)
{
    const static QBrush brush[2] = { Qt::gray, Qt::yellow };
    const static QPen pens[2] = { QPen(QColor(), 0, Qt::NoPen), QPen(QColor(255, 0, 255), 1, Qt::SolidLine) };

    for (const auto &t : m_transdefs)
    {
        // Speed up rendering by clipping to the viewport's image rectangle
        if (t.box.intersected(viewport) != QRect())
        {
            bool state = ::controller.getSimx().getNetState(t.gatenode);
            painter.setPen(pens[state]);
            painter.setBrush(brush[state || highlightAll]);
            painter.drawPath(t.path);
        }
    }
}

/******************************************************************************
 * Experimental code
 ******************************************************************************/

/*
 * Scan a feature bitmap for the next transistor area, return false when we traverse the complete map
 */
bool ClassChip::scanForTransistor_4(uchar const *p, uint &offset)
{
    // If we land on a transistor bit, skip the transistor by its width
    while (p[offset] & TRANSISTOR)
        offset++;
    // Clean start on a not-a-transistor bit, find the next transistor or the end of the map
    while (!(p[offset] & TRANSISTOR) && (offset < m_mapsize))
        offset++;
    return offset < m_mapsize;
}

/*
 * Run with the command "ex(4)"
 * Creates transistors paths based on our feature bitmap.
 * There are many more (technically) transistors which are removed from the netlist because they are
 * either pull-ups or otherwise non-functional transistors. This function will detect all of them and mark
 * them on a bitmap created.
 * The last part of this function simply refers to the transdef's transistor data (their bounding boxes) to
 * assign the transistor outline paths to each.
 */
void ClassChip::experimental_4()
{
    qInfo() << "Experimental: create transistor paths; transistors' locations scanned from feature bitmap";
    QEventLoop e; // Don't freeze the GUI
    int c = 0;

    bool ok = true;
    // Shallow copy constructor, will create a new image once data buffer is written to
    QImage img(getImage("bw.featuremap", ok));
    Q_ASSERT(ok);

    // Render our transistors (paths) into an image we can see
    QPainter painter(&img);
    painter.setPen(QPen(QColor(), 0, Qt::NoPen)); // No outlines
    painter.setBrush(QColor(255, 255, 255)); // 0xFFFFFFFF  Set this color so we can skip already detected transistors

    QVector<QPainterPath> paths;

    uchar const *p = img.constBits();
    uint offset = 0;
    while (scanForTransistor_4(p, offset))
    {
        uint x = offset % m_sx;
        uint y = offset / m_sx;

        // Skip already detected and rendered transistor
        if (img.pixel(x,y) == 0xFFFFFFFF)
            continue;

        // Build the path around a transistor
        paths.append(QPainterPath(QPointF(x, y)));
        edgeWalk(p, paths.last(), x, y);

        // Render that transistor into our image
        painter.drawPath(paths.last());

        if ((++c % 1000) == 0)
        {
            qDebug() << "Transistors:" << paths.count() << " x:" << x << "y:" << y;
            e.processEvents(QEventLoop::AllEvents); // Don't freeze the GUI
        }
    }
    qDebug() << "Transistors mapped:" << paths.count();

    //-----------------------------------------------------------------------------------------
    // Assign our outline paths into m_transdefs transistors for which the bounding box matches
    //-----------------------------------------------------------------------------------------
    for (const auto &path : paths)
    {
        for (auto &t : m_transdefs)
        {
            if (path.boundingRect() == t.box)
            {
                t.path = path;
                break;
            }
        }
    }
    qDebug() << "Finished";

    img.setText("name", "bw.transistors4");
    m_img.append(img);
}
