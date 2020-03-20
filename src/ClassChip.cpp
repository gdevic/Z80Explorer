#include "ClassChip.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QPainter>

// List of z80 chip resource images / layers. "Z80_" and ".png" are appended only when loading the files.
static const QStringList layers =
{
    { "diffusion" },    // 0
    { "polysilicon" },  // 1
    { "metal" },        // 2
    { "buried" },       // 3
    { "vias" },         // 4
    { "ions" },         // 5
    { "pads" },         // 6
    { "metal_VCC_GND" },// 7
    { "vias_VCC_GND" }, // 8
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
 * Returns a list of layer / image names
 */
const QStringList ClassChip::getLayerNames()
{
    QStringList l = layers;
    l.append("transistors"); // XXX hack
    return l;
}

/*
 * Load chip images
 */
bool ClassChip::loadImages(QString dir)
{
    QEventLoop e; // Don't freeze the GUI
    QImage img;
    m_img.clear();
    for (auto image : layers)
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
        new_images.append(image.convertToFormat(QImage::Format_Grayscale8, Qt::AutoColor));
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
    QImage m_trans(m_img[0].width(), m_img[0].height(), QImage::Format_ARGB32);
    drawTransistors(m_trans);
    m_trans.setText("name", "transistors");
    m_img.append(m_trans);
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
 * Attempts to load all chip resource that we expect to have
 */
bool ClassChip::loadChipResources(QString dir)
{
    qInfo() << "Loading chip resources from " << dir;
    if (loadImages(dir) && loadNodenames(dir) && loadTransdefs(dir) && addTransistorsLayer() && convertToGrayscale())
    {
        m_dir = dir;
        emit refresh(); // XXX do we still need this?
        qInfo() << "Completed loading chip resources";
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
