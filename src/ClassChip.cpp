#include "ClassChip.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QPainter>

static const QStringList res_images =
{
    { "Z80_buried.png" },
    { "Z80_diffusion.png" },
    { "Z80_ions.png" },
    { "Z80_metal.png" },
    { "Z80_pads.png" },
    { "Z80_polysilicon.png" },
    { "Z80_vias.png" },
    { "z80_metal_VCC_GND.png" },
    { "z80_vias_VCC_GND.png" },
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
    static QImage img_empty;
    if (i < uint(m_img.count()))
    {
        m_last_image = i;
        return m_img[i];
    }
    return img_empty;
}

QImage &ClassChip::getLastImage()
{
    static QImage img_empty;
    if (m_last_image < uint(m_img.count()))
        return m_img[m_last_image];
    return img_empty;
}

/*
 * Load chip images
 */
bool ClassChip::loadImages(QString dir)
{
    QEventLoop e; // Don't freeze the GUI
    QImage img;
    m_img.clear();
    for (auto image : res_images)
    {
        QString png_file = dir + "/" + image;
        qInfo() << "Loading " + png_file;
        e.processEvents(QEventLoop::AllEvents); // Don't freeze the GUI
        if (img.load(png_file))
        {
            int w = img.width();
            int h = img.height();
            int d = img.depth();
            QImage::Format f = img.format();
            qInfo() << "Image w=" << w << " h=" << h << " depth=" << d << "format=" << f;
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
 * Attempts to load all chip resource that we expect to have
 */
bool ClassChip::loadChipResources(QString dir)
{
    qInfo() << "Loading chip resources from " << dir;
    if (loadImages(dir) && convertToGrayscale() && loadNodenames(dir))
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
                //qDebug() << list;
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
                painter.drawPath(s.path);

                segdefs.append(s);
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
    emit refresh();
}

QList<int> ClassChip::getNodesAt(int x, int y)
{
    QList<int> list;
    int i = 0; // tmp to help out finding erroneous lines of data
    for(auto s : segdefs)
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

QList<QString> ClassChip::getNodenamesFromNodes(QList<int> nodes)
{
    QList<QString> list;
    for(auto i : nodes)
    {
        if (m_nodenames.contains(i))
            list.append(m_nodenames[i]);
    }
    return list;
}
