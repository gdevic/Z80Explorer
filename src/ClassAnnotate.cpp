#include "ClassAnnotate.h"
#include "ClassController.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ClassAnnotate::ClassAnnotate(QObject *parent) : QObject(parent)
{
    // Set the font family of the annotation text
    m_fixedFont = QFont("Consolas");
}

void ClassAnnotate::onShutdown()
{
    save(m_jsonFile);
}

/*
 * Adds annotation to the list
 */
void ClassAnnotate::add(QString text, QRect box)
{
    annotation a(text, box);

    qreal pixX = qreal(box.width()) / (textLength(a.text) / m_someXFactor);
    qreal pixY = box.height();
    qreal pix = qMin(pixX, pixY);

    a.pix = pix;
    a.pos = box.topLeft();

    if (pixX < pixY) // Center the text vertically
        a.pos += QPoint(0, (pixY - pixX) / 2);

    m_annot.append(a);
}

/*
 * Returns a list of annotation indices at the given coordinate
 */
QVector<uint> ClassAnnotate::get(QPoint &pos)
{
    QVector<uint> sel;
    for (int i = 0; i < m_annot.count(); i++)
    {
        annotation &a = m_annot[i];
        if (a.rect.contains(pos))
            sel.append(i);
    }
    return sel;
}

/*
 * Returns a list of annotation indices within the given rectangle
 */
QVector<uint> ClassAnnotate::get(QRect r)
{
    QVector<uint> sel;
    for (int i = 0; i < m_annot.count(); i++)
    {
        annotation &a = m_annot[i];
        if (r.contains(a.rect))
            sel.append(i);
    }
    return sel;
}

/*
 * Draws annotations
 * scale is the current image scaling value, used to selectively draw different text sizes
 */
void ClassAnnotate::draw(QPainter &painter, const QRect &viewport, qreal scale)
{
    const uint hcycle = ::controller.getSimZ80().getCurrentHCycle() - 1;
    QPen pens[] { QPen(Qt::white), QPen(Qt::black) };
    for (auto &a : m_annot)
    {
        // Annotations outside the image area are always shown, and drawn with black pen
        bool isOutside = !m_imgRect.intersects(a.rect);
        QPen &pen = pens[isOutside];
        painter.setPen(pen);
        // Selective rendering hides annotations that are too large or too small for the given scale
        qreal apparent = a.pix * scale;
        bool show = isOutside || ((apparent < 200) && (apparent > 8));
        // Speed up rendering by clipping all rectangles that are outside our viewport
        if (a.rect.intersected(viewport) == QRect())
            show = isOutside; // except those that are outside the chip image

        // First, dim the area rectangle proportional to the scale and the text size
        if (show && a.drawrect)
        {
            painter.save();
            painter.setBrush(QColor(0,0,0, qBound(0.0, apparent * 2 + 50, 255.0)));
            painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            painter.drawRect(a.rect);
            painter.restore();
        }

        // Draw the overline (bar) if needed
        if (show && a.overline)
        {
            qreal thickness = a.pix / 10.0;
            pen.setWidthF(thickness);
            painter.setPen(pen);
            qreal dx = a.pix * textLength(a.text) / m_someXFactor;
            painter.drawLine(a.pos + QPoint(thickness, thickness), a.pos + QPoint(dx - thickness, thickness));
        }

        // Draw the text of the annotation
        if (show)
        {
            m_fixedFont.setPixelSize(a.pix); // Set the text size
            painter.setFont(m_fixedFont);

            // Macro substitution: early exit if there are no macro delimiters
            QString finalText = a.text;
            if (a.text.indexOf('{') >= 0)
            {
                // Net and bus names should be enclosed in {...} for the substitution to take place
                QStringList tokens = a.text.split('{');
                finalText.clear();
                for (auto &s : tokens)
                {
                    int i = s.indexOf('}'); // Find the end delimiter
                    if (i > 0)
                    {
                        QString name = s.mid(0, i).trimmed(); // Extract the name

                        watch *w = ::controller.getWatch().find(name);
                        pin_t data_cur = ::controller.getWatch().at(w, hcycle);
                        if (data_cur < 2)
                            finalText.append(QString::number(data_cur));
                        else if (data_cur == 2)
                            finalText.append("Z");
                        else if (data_cur == 3)
                            finalText.append("?");
                        else if (data_cur == 4) // Bus
                        {
                            uint width, value = ::controller.getWatch().at(w, hcycle, width);
                            if (width)
                                finalText.append(::controller.formatBus(ClassController::FormatBus::Hex, value, width, false));
                            else
                                finalText.append("X");
                        }
                        else
                            finalText.append("error");
                        finalText.append(s.midRef(i+1));
                    }
                    else
                        finalText.append(s);
                }
            }
            // Cache output QStaticText so we don't have to change it (rebuild it) unless we have to (due to a macro substitution)
            if (finalText != a.cache.text())
                a.cache.setText(finalText);
            painter.drawStaticText(a.pos, a.cache);
        }

        // Finally, draw the outline of a rectangle with a line less thick than the overline
        // We will draw the outlines at any scale factor to hint that there are annotations there
        if (a.drawrect)
        {
            pen.setWidthF(show ? a.pix / 15.0 : 1);
            painter.setPen(pen);
            painter.drawRoundedRect(a.rect, 5, 5);
        }
    }
}

/*
 * Loads user annotations from a file
 */
bool ClassAnnotate::load(QString fileName)
{
    if (m_jsonFile.isEmpty()) // Set the initial file name
        m_jsonFile = fileName;
    else
        save(m_jsonFile);     // Otherwise, save current annotations so the changes are not lost
    qInfo() << "Loading annotations from" << fileName;
    QFile loadFile(fileName);
    if (loadFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(data));
        const QJsonObject json = loadDoc.object();

        if (json.contains("annotations") && json["annotations"].isArray())
        {
            QJsonArray array = json["annotations"].toArray();
            m_annot.clear();
            m_annot.reserve(array.size());

            for (int i = 0; i < array.size(); i++)
            {
                annotation a;
                QJsonObject obj = array[i].toObject();
                if (obj.contains("text") && obj["text"].isString())
                    a.text = obj["text"].toString();
                if (obj.contains("x") && obj["x"].isDouble())
                    a.pos.setX(obj["x"].toInt());
                if (obj.contains("y") && obj["y"].isDouble())
                    a.pos.setY(obj["y"].toInt());
                if (obj.contains("rx") && obj["rx"].isDouble())
                    a.rect.setX(obj["rx"].toInt());
                if (obj.contains("ry") && obj["ry"].isDouble())
                    a.rect.setY(obj["ry"].toInt());
                if (obj.contains("rw") && obj["rw"].isDouble())
                    a.rect.setWidth(obj["rw"].toInt());
                if (obj.contains("rh") && obj["rh"].isDouble())
                    a.rect.setHeight(obj["rh"].toInt());
                if (obj.contains("pix") && obj["pix"].isDouble())
                    a.pix = obj["pix"].toInt();
                if (obj.contains("bar") && obj["bar"].isBool())
                    a.overline = obj["bar"].toBool();
                if (obj.contains("rect") && obj["rect"].isBool())
                    a.drawrect = obj["rect"].toBool();
                m_annot.append(a);
            }
            m_jsonFile = fileName;
            return true;
        }
        else
            qWarning() << "Invalid json file";
    }
    else
        qWarning() << "Unable to load" << fileName;
    return false;
}

/*
 * Saves user annotations to a file
 */
bool ClassAnnotate::save(QString fileName)
{
    qInfo() << "Saving annotations to" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
    {
        QJsonObject json;
        QJsonArray jsonArray;
        for (annotation &a : m_annot)
        {
            QJsonObject obj;
            obj["text"] = a.text;
            obj["x"] = a.pos.x();
            obj["y"] = a.pos.y();
            obj["rx"] = a.rect.x();
            obj["ry"] = a.rect.y();
            obj["rw"] = a.rect.width();
            obj["rh"] = a.rect.height();
            obj["pix"] = int(a.pix);
            obj["bar"] = a.overline;
            obj["rect"] = a.drawrect;
            jsonArray.append(obj);
        }
        json["annotations"] = jsonArray;

        QJsonDocument saveDoc(json);
        saveFile.write(saveDoc.toJson());
        return true;
    }
    else
        qWarning() << "Unable to save" << fileName;
    return false;
}
