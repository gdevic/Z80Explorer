#include "ClassChip.h"
#include "ClassController.h"
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSettings>
#include <QtConcurrent>

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
        // If we cannot load the layer map, we need to create it, but we can create only a partial layer map
        if (!loadLayerMap(dir))
        {
#if HAVE_PREBUILT_LAYERMAP
            qCritical() << "Prebuilt layermap missing!";
            return false;
#endif
            fillLayerMap(); // Generates a partial layer map; limited inspection functionality
            saveLayerMap(); // Saves partial layer map to a file to be loaded next time
        }
        createLayerMapImage("vss.vcc", true); // Create a base image showing GND and +5V traces
        drawAllNetsAsInactive("vss.vcc", "vss.vcc.nets"); // Using the vss.vcc as a base, faintly add all nets
        redrawNetsColorize("vss.vcc", "vss.vcc.nets.col"); // Using the vss.vcc as a base, colorize selected buses
        connect(&::controller, &ClassController::eventNetName, this, [this]() // May need different colors for renamed nets
            { redrawNetsColorize("vss.vcc", "vss.vcc.nets.col"); } ); // Dynamically rebuild the colorized image
        detectLatches(); // Detect latches and load custom latch definitions
        experimental_4(); // Create transistor paths

        setFirstImage("vss.vcc.nets.col");
        setFirstImage("vss.vcc.nets");

        qInfo() << "Completed loading chip resources";
        return true;
    }
    qCritical() << "Loading chip resource failed";
    return false;
}

/*
 * Load chip images
 */
bool ClassChip::loadImages(QString dir)
{
    // List of z80 chip resource images / layers. "Z80_" and ".png" are appended only when loading the files
    // since we use those names, as layer names, to store them into QImage.text fields
    static const QStringList files =
    {
        "diffusion",
        "polysilicon",
        "metal",
        "buried",
        "vias",
        "ions"
    };

    QEventLoop e; // Don't freeze the GUI
    QImage img;
    m_img.clear();
    for (auto &image : files)
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
            qCritical() << "Error loading" + image;
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
 * Loads segdefs.js and segvdefs
 * The first file contains segment definitions from the Visual 6502 team for this processor
 * I had processed that file further to merge each of the nets into single path and we use that
 * as the alternate visual segment representation
 */
bool ClassChip::loadSegdefs(QString dir)
{
    if (!loadSegdefsJs(dir))
        return false;
    // Load processed and smoother paths; don't care if the file "segvdefs.bin" does not exist
    loadSegvdefs(dir);
    return true;
}

/*
 * Loads segdefs.js legacy segment defintion file
 */
bool ClassChip::loadSegdefsJs(QString dir)
{
    QString segdefs_file = dir + "/segdefs.js";
    qInfo() << "Loading" << segdefs_file;
    QFile file(segdefs_file);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        m_segvdefs.clear();
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
                    segvdef s;
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

                    if (!m_segvdefs.contains(key))
                    {
                        s.netnum = key;
                        s.paths.append(path);
                        m_segvdefs[key] = s;
                    }
                    else
                        m_segvdefs[key].paths.append(path);
                }
                else
                    qWarning() << "Invalid line" << line;
            }
            else
                qDebug() << "Skipping" << line;
        }
        qInfo() << "Loaded" << m_segvdefs.count() << "segment visual definitions";
        return true;
    }
    else
        qCritical() << "Error opening segdefs.js";
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
        m_transvdefs.clear();
        uint y = m_img[0].height() - 1;
        while(!in.atEnd())
        {
            line = in.readLine();
            if (line.startsWith('['))
            {
                line.replace('[', ' ').replace(']', ' '); // Make it a simple list of numbers
                line.chop(2);
                list = line.split(QLatin1Char(','), Qt::SkipEmptyParts);
                if (list.length()==14 && list[0].length() > 2)
                {
                    // ----- Add the transistor to the transistor array -----
                    QString tnum = list[0].mid(3, list[0].length() - 4);
                    tran_t i = tnum.toUInt();
                    Q_ASSERT(i < MAX_TRANS);

                    transvdef t;
                    t.id = i;
                    t.gatenet = list[1].toUInt();
                    // The order of values in the data file is: [4,5,6,7] => left, right, bottom, top
                    // The Y coordinates in the input data stream are inverted, with 0 starting at the bottom
                    t.box = QRect(QPoint(list[4].toInt(), y - list[7].toInt()), QPoint(list[5].toInt() - 1, y - list[6].toInt() - 1));

                    m_transvdefs.append(t);
                }
                else
                    qWarning() << "Invalid line" << list;
            }
            else
                qDebug() << "Skipping" << line;
        }
        qInfo() << "Loaded" << m_transvdefs.count() << "transistor visual definitions";
        return true;
    }
    else
        qCritical() << "Error opening transdefs.js";
    return false;
}

/*
 * Returns a reference to the image by the image index.
 * Returns the very first image if the index is outside the stored images count
 */
QImage &ClassChip::getImage(uint i)
{
    if (i < uint(m_img.count()))
        return m_img[i];
    return m_img[0];
}

/*
 * Returns a reference to the image by the image (embedded) name
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
const QStringList ClassChip::getImageNames()
{
    QStringList names;
    for (auto &image : m_img)
        names.append(image.text("name"));
    return names;
}

/*
 * Returns a list of (unique) nets located at the specified image coordinates
 */
template<bool includeVssVcc>
const QVector<net_t> ClassChip::getNetsAt(int x, int y)
{
    QVector<net_t> list;
    if ((uint(x) >= m_sx) || (uint(y) >= m_sy))
        return list;
    // Use our layer map to read vss, vcc since they are the largest, already mapped, areas
    uint offset = x + y * m_sx;
#if HAVE_PREBUILT_LAYERMAP
    const net_t minNet = includeVssVcc ? 0 : 2;
    net_t l0 = m_p3[0][offset];
    net_t l1 = m_p3[1][offset];
    net_t l2 = m_p3[2][offset];

#if FIX_Z80_LAYERMAP_TO_VISUAL_ENUM
    // Net values in the layermap file are generated by Z80Simulator program. The values should match
    // the visual net polygon descriptions but they are off by 1 in between nets 1559 and 1710 (inclusive)
    // This fixes up values in that range so that the correct net number is returned.
    if ((l0 > 1558) && (l0 < 1710)) l0++;
    if ((l1 > 1558) && (l1 < 1710)) l1++;
    if ((l2 > 1558) && (l2 < 1710)) l2++;
#endif

    if (l0 > minNet) list.append(l0);
    if ((l1 > minNet) && (!list.contains(l1))) list.append(l1);
    if ((l2 > minNet) && (!list.contains(l2))) list.append(l2);
#else
    if (includeVssVcc)
    {
        if ((m_p3[0][offset] | m_p3[1][offset] | m_p3[2][offset]) == 1) list.append(1); // vss
        if ((m_p3[0][offset] | m_p3[1][offset] | m_p3[2][offset]) == 2) list.append(2); // vcc
    }
    for (const auto &s : m_segvdefs)
    {
        if (s.netnum > 2) // Skip vss and vcc segments
        {
            for (const auto &path : s.paths)
            {
                if (path.contains(QPointF(x, y)) && !list.contains(s.netnum))
                    list.append(s.netnum);
            }
        }
    }
#endif // HAVE_PREBUILT_LAYERMAP
    return list;
}

// Explicit instantiation so we don't have to keep the templated code in a header file
template const QVector<net_t> ClassChip::getNetsAt<true>(int, int);
template const QVector<net_t> ClassChip::getNetsAt<false>(int, int);

/*
 * Returns the segment visual definition, zero if not found
 */
const segvdef *ClassChip::getSegment(net_t net)
{
    static const segvdef empty;
    if (use_alt_segdef && m_segvdefs2.contains(net))
        return &m_segvdefs2[net];
    else
        return m_segvdefs.contains(net) ? &m_segvdefs[net] : &empty;
}

/*
 * Returns transistor visual definition, nullptr if not found
 */
const transvdef *ClassChip::getTrans(tran_t id)
{
    if ((id < MAX_TRANS) && id)
    {
        for (int i=0; i<m_transvdefs.size(); i++)
        {
            if (m_transvdefs.at(i).id == id)
                return &m_transvdefs.at(i);
        }
    }
    return nullptr;
}

/*
 * Returns a transistor at the specified image coordinates or 0 for no transistor
 */
tran_t ClassChip::getTransistorAt(int x, int y)
{
    if ((uint(x) >= m_sx) || (uint(y) >= m_sy))
        return 0;
    // Early exit if there are no transistors at this location
    uint offset = x + y * m_sx;
    if (m_fmap[offset] & TRANSISTOR)
    {
        for (auto &s : m_transvdefs)
        {
            if (s.path.contains(QPoint(x, y)))
                return s.id;
        }
    }
    return 0;
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
    for (auto &s : m_transvdefs)
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
    for (auto &image : m_img)
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
                qWarning() << "Error reading" << fileName << "layer" << i;
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
    m_fmap = new uchar[m_mapsize];

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
//        viad = c & VIA_DIFF; // XXX ?
//        viap = c & VIA_POLY;

        // Mark a transistor area (poly over diffusion without a buried contact)
        // Transistor path also splits the diffusion area into two, so we remove DIFF over these traces
        if ((c & (DIFF | POLY | BURIED)) == (DIFF | POLY)) c = (c & ~DIFF) | TRANSISTOR;

        m_fmap[i] = c;
    }
    QImage featuremap(m_fmap, m_sx, m_sy, m_sx * sizeof(uint8_t), QImage::Format_Grayscale8, [](void *p){ delete[] static_cast<uchar *>(p); }, (void *)m_fmap);
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
void ClassChip::createLayerMapImage(QString name, bool onlyVssVcc)
{
    // Out of 3 layers, compose one visual image that we'd like to see
    uint16_t *p = new uint16_t[m_mapsize];

    for (uint i = 0; i < m_mapsize; i++)
    {
        uint16_t net[3] { m_p3[0][i], m_p3[1][i], m_p3[2][i] };
        uint16_t c = 0;
        if ((net[0] == 1) || (net[1] == 1) || (net[2] == 1)) c = ::controller.getColors().get16(1); // vss
        if ((net[0] == 2) || (net[1] == 2) || (net[2] == 2)) c = ::controller.getColors().get16(2); // vcc
        if (!onlyVssVcc)
            c = (net[0] << 4) | (net[1] << 2) | (net[2]); // Visualize all layermap net values
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

    for (uint i=3; i<::controller.getSimZ80().getNetlistCount(); i++)
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

    for (uint i=3; i<::controller.getSimZ80().getNetlistCount(); i++)
    {
        if (::controller.getColors().isDefined(i))
            painter.setBrush(::controller.getColors().get(i));
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
 * This may only run if HAVE_PREBUILT_LAYERMAP is set to 0
 *
 * Creates a partial m_p3 (layer map) from "bw.featuremap2" feature map.
 * Since this process takes a long time, only vss,vcc are filled in.
 * However, the fully filled layer map was pre-generated by Z80Simulator code
 * and provided as file "layermap.bin". That one contains all the nets.
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
 * Fills layer map (this class' m_p3) from the feature bitmap (given by p_map pointer)
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

    qDebug() << "Layer map build for net" << id << "took" << timer.elapsed() << "ms";
}

/*
 * Fills layer map with vss and vcc
 */
void ClassChip::fillLayerMap()
{
    qInfo() << "Fill layer map with vss and vcc";

    drawFeature(100,100, 2, 1); // vss
    drawFeature(4456,2512, 2, 2); // vcc
}

/*
 * Saves layer map to a file
 */
void ClassChip::saveLayerMap()
{
    QSettings settings;
    QString fileName = settings.value("ResourceDir").toString() + "/layermap.bin";
    qInfo() << "Saving layer map to" << fileName;
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly))
    {
        int64_t size = m_mapsize * sizeof(uint16_t); // Size of one layer
        for (uint i = 0; i < 3; i++)
        {
            int64_t written = file.write((const char *) m_p3[i], size);
            if (written != size)
            {
                qWarning() << "Error writing" << fileName << "layer" << i;
                return;
            }
        }
    }
    else
        qWarning() << "Unable to save" << fileName;
}

/****************************************************************************************
 * Detect latches
 * This code detects immediate loops in transistor gate nets to its source/drain nets
 * via one extra net (obtained by calling netsDriven). This detects a good number
 * of latches - but not all: some latches are separated by 2 nets/transistor steps,
 * such are eight Instruction Register latches (which have an additional driver/inverter
 * in the loop). We append those by reading a resource file "latches.ini"
 ****************************************************************************************/

void ClassChip::detectLatches()
{
    m_latches.clear();
    for (auto &t : m_transvdefs)
    {
        net_t c1c2[2];
        bool validnet = ::controller.getNetlist().getTnet(t.id, c1c2[0], c1c2[1]);
        if (validnet)
        {
            const QVector<net_t> driven = ::controller.getNetlist().netsDriven(t.gatenet);
            int index = -1;              // Consider all nets except gnd, vcc and clock: they cannot form a latch
            if (driven.contains(c1c2[0]) && (c1c2[0] > 3)) index = 0;
            if (driven.contains(c1c2[1]) && (c1c2[1] > 3)) index = 1;

            if (index >= 0)
            {
                bool completed = false;
                for (auto &l : m_latches)
                {
                    if (l.n1 == c1c2[index])
                    {
                        l.t2 = t.id;
                        completed = true;
                        break;
                    }
                }
                if (!completed)
                {
                    latchdef latch;
                    latch.t1 = t.id;
                    latch.n1 = t.gatenet;
                    latch.t2 = 0;
                    latch.n2 = c1c2[index];

                    m_latches.append(latch);
                }
            }
        }
    }
    qDebug() << "Detected" << m_latches.count() << "latches";

    loadLatches();

    // Initialize latch bounding boxes - spanning both latch transistors
    for (auto &latch : m_latches)
    {
        if (latch.t2 == 0)
        {
            qDebug() << "Not complete latch at transistor" << latch.t1;
            latch.box = QRect();
        }
        else
        {
            // Create a bounding box around the two transistors
            const transvdef *vdef1 = getTrans(latch.t1);
            const transvdef *vdef2 = getTrans(latch.t2);
            Q_ASSERT(vdef1 && vdef2);
            latch.box = vdef1->box.united(vdef2->box);
        }
        // Remove latches that are visually too large
        if (latch.box.width() > 150 || latch.box.height() > 150)
        {
            qDebug() << "Removing latch bounds" << latch.box;
            latch.box = QRect();
        }
    }
}

bool ClassChip::loadLatches()
{
    QSettings settings;
    QString fileName = settings.value("ResourceDir").toString() + "/latches.ini";
    qInfo() << "Loading" << fileName;
    QFile file(fileName);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&file);
        QString line;
        QStringList list;
        uint count = 0, ln = 0;
        while(!in.atEnd())
        {
            line = in.readLine(); ln++;
            line = line.left(line.indexOf(';')).trimmed();
            if (line.length())
            {
                list = line.split(QLatin1Char(','), Qt::SkipEmptyParts);
                if (list.count() == 2)
                {
                    uint t1, t2;
                    bool ok1, ok2;
                    t1 = list[0].toUInt(&ok1);
                    t2 = list[1].toUInt(&ok2);
                    if (ok1 && ok2)
                    {
                        const transvdef *vdef1 = getTrans(t1);
                        const transvdef *vdef2 = getTrans(t2);
                        if (vdef1 != nullptr)
                        {
                            if (vdef2 != nullptr)
                            {
                                latchdef latch;
                                latch.t1 = t1;
                                latch.n1 = vdef1->gatenet;
                                latch.t2 = t2;
                                latch.n2 = vdef2->gatenet;

                                m_latches.append(latch);
                                count++;
                            }
                            else
                                qWarning() << "Line" << ln << "invalid transistor number" << t2;
                        }
                        else
                            qWarning() << "Line" << ln << "invalid transistor number" << t1;
                    }
                    else
                        qWarning() << "Line" << ln << "unable to parse transistor numbers";
                }
                else
                    qWarning() << "Line" << ln << "unable to parse transistor numbers";
            }
        }
        qInfo() << "Loaded" << count << "custom latch definitions";
        return true;
    }
    else
        qCritical() << "Error opening latches.ini";
    return false;
}

void ClassChip::drawLatches(QPainter &painter, const QRect &viewport)
{
    painter.setBrush(Qt::blue);
    painter.setPen(Qt::yellow);
    for (auto &l : m_latches)
    {
        // Speed up rendering by clipping to the viewport's image rectangle
        if (l.box.intersected(viewport) != QRect())
            painter.drawRect(l.box);
    }
}

/*
 * Returns true if a net is part of any latch
 */
bool ClassChip::isLatch(net_t net)
{
    auto i = std::find_if(m_latches.begin(), m_latches.end(), [net](latchdef &l) { return (l.n1 == net) || (l.n2 == net); });
    return (i != m_latches.end());
}

/*
 * Returns latch transistors and nets
 */
void ClassChip::getLatch(net_t net, tran_t &t1, tran_t &t2, net_t &n1, net_t &n2)
{
    (void) std::find_if(m_latches.begin(), m_latches.end(), [net,&t1,&t2,&n1,&n2](latchdef &l)
    { t1 = l.t1; t2 = l.t2; n1 = l.n1; n2 = l.n2; return (l.n1 == net) || (l.n2 == net); });
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
    if (n==2) return;
    if (n==3) return experimental_3();
    if (n==4) return experimental_4();
    qWarning() << "Invalid experimental function index" << n;
}

/******************************************************************************
 * Experimental code
 ******************************************************************************/

/*
 * Merges net paths for a better visual display
 * This code merges paths for each net so nets look better, but this process can take > 2 min on a fast PC
 * Hence, the merged nets have been cached in the file "segvdefs.bin"
 */
void ClassChip::experimental_1()
{
    qInfo() << "Experimental: merge visual segment paths. This process is running in the background and may take a few minutes to complete.";
    qInfo() << "After it is done (you should see the message 'Saving segvdefs'), restart the application to use the new, smoother, paths.";

    // Code in this block will run in another thread
    QFuture<void> future = QtConcurrent::run([=]()
    {
        QPainterPath path;
        for (auto &seg : m_segvdefs)
        {
            path.clear();
            for (auto &p : seg.paths)
                path |= p;
            seg.paths.clear();
            seg.paths.append(path.simplified());
        }

        // Save created visual paths to a file
        {
            QSettings settings;
            QString resDir = settings.value("ResourceDir").toString();
            saveSegvdefs(resDir);
        }
    });
}

/*
 * Saves visual paths to a file
 */
bool ClassChip::saveSegvdefs(QString dir)
{
    QString fileName = dir + "/segvdefs.bin";
    qInfo() << "Saving segvdefs to" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly))
    {
        QDataStream out(&saveFile);
        out << m_segvdefs.count();
        for (auto &seg : m_segvdefs)
        {
            out << seg.netnum;
            out << seg.paths.count();
            for (auto &path : seg.paths)
                out << path;
        }
        return true;
    }
    qWarning() << "Unable to save" << fileName;
    return false;
}

/*
 * Loads visual paths from a file
 */
bool ClassChip::loadSegvdefs(QString dir)
{
    QString fileName = dir + "/segvdefs.bin";
    qInfo() << "Loading segvdefs from" << fileName;
    QFile loadFile(fileName);
    if (loadFile.open(QIODevice::ReadOnly))
    {
        m_segvdefs2 = m_segvdefs;

        try // May throw an exception if the data is not formatted exactly as expected
        {
            QDataStream in(&loadFile);
            int count, num_paths;
            in >> count;
            if (count == m_segvdefs2.count())
            {
                while (count-- > 0)
                {
                    segvdef segvdef;
                    in >> segvdef.netnum >> num_paths;
                    while (num_paths-- > 0)
                    {
                        QPainterPath path;
                        in >> path;
                        segvdef.paths.append(path);
                    }

                    if (m_segvdefs2.contains(segvdef.netnum))
                        m_segvdefs2.remove(segvdef.netnum);
                    m_segvdefs2[segvdef.netnum] = segvdef;
                }
                return true;
            }
            qWarning() << "Incorrect number of segvdefs!";
        } catch (...)
        {
            qWarning() << "Invalid data format";
        }
    }
    qWarning() << "Unable to load" << fileName;
    return false;
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
        // Add offset dx,dy to make the transistor shape fit better onto the existing segments
        // The segdefs that we use as a background have extra pixel on the right and bottom edges
        static const uint dx[8] = { 0,0,0,1,1,1,1,0 }; // correction for directions 3, 4, 5 and 6
        static const uint dy[8] = { 1,0,0,0,0,1,1,1 }; // correction for directions 5, 6, 7 and 0
        path.lineTo(x + dx[nextdir], y + dy[nextdir]);

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
    for (auto &t : m_transvdefs)
    {
        // Find the top-leftmost edge of a transistor
        if (!scanForTransistor(p, t.box, x, y)) // There are few trans in Visual 6502 transdefs.js that are...not (?)
        {
            qWarning() << "Unable to scan transistor" << t.id << t.box;
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

    for (auto &t : m_transvdefs)
    {
        // Speed up rendering by clipping to the viewport's image rectangle
        if (t.box.intersected(viewport) != QRect())
        {
            bool state = ::controller.getSimZ80().getNetState(t.gatenet);
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

    //------------------------------------------------------------------------------------------
    // Assign our outline paths into m_transvdefs transistors for which the bounding box matches
    //------------------------------------------------------------------------------------------
    for (const auto &path : paths)
    {
        for (auto &t : m_transvdefs)
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

/******************************************************************************
 * Experimental code
 ******************************************************************************/

/*
 * Dynamically write nearby net names (experimental)
 */
void ClassChip::expDynamicallyNameNets(QPainter &painter, const QRect &viewport, qreal scale)
{
    if (scale < 1.5) // Start naming nets only at the certain level of detail
        return;

    painter.setFont(m_fixedFont);
    painter.setPen(QPen(Qt::white, 1, Qt::SolidLine));

    int x = viewport.x() + (viewport.width() / 3);
    net_t lastNet = 0;
    for (int y = viewport.bottom(); y > viewport.top(); y--)
    {
        QVector<net_t> nets = getNetsAt<false>(x, y);
        if ((nets.count() == 1) && (nets[0] != lastNet))
        {
            net_t net = nets[0];
            painter.drawText(x, y, ::controller.getNetlist().get(net));
            lastNet = net;
            y -= 8;
        }
    }
}
