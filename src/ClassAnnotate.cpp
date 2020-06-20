#include "ClassAnnotate.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

ClassAnnotate::ClassAnnotate(QObject *parent) : QObject(parent)
{
    m_fixedFont = QFont("Consolas");
}

void ClassAnnotate::onShutdown()
{
    QSettings settings;
    QString resDir = settings.value("ResourceDir").toString();
    if (m_annot.count()) // Save the annotations only if we have any defined
        save(resDir);
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
        QRect r = QRect(a.pos.x(), a.pos.y(), a.pix * textLength(a.text) / m_someXFactor, a.pix);
        if (r.contains(pos))
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
        if (r.contains(a.pos))
            sel.append(i);
    }
    return sel;
}

/*
 * Draws annotations
 * scale is the current image scaling value, used to selectively draw different text sizes
 */
void ClassAnnotate::draw(QPainter &painter, qreal scale)
{
    QPen pen(Qt::white);
    painter.setPen(pen);
    for (auto &a : m_annot)
    {
        // Selective rendering hides annotations that are too large or too small for the given scale
        qreal apparent = a.pix * scale;
        if (apparent > 200 || apparent < 8)
            continue;

        // Draw the overline (bar) if needed
        if (a.overline)
        {
            qreal thickness = a.pix / 10.0;
            pen.setWidthF(thickness);
            painter.setPen(pen);
            qreal dx = a.pix * textLength(a.text) / m_someXFactor;
            painter.drawLine(a.pos + QPoint(thickness, thickness), a.pos + QPoint(dx - thickness, thickness));
        }

        // drawStaticText() anchor is at the top-left point (drawText() is on the bottom-left)
        m_fixedFont.setPixelSize(a.pix); // Base all text on the same font family; set the size
        painter.setFont(m_fixedFont);
        //painter.drawRect(a.pos.x(), a.pos.y(), a.pix * textLength(a.text) / m_someXFactor, a.pix); // Testing: Draw box around the text
        painter.drawStaticText(a.pos, a.text);
        if (a.drawrect)
            painter.drawRect(a.rect);
    }
}

/*
 * Loads user annotations from a file
 */
bool ClassAnnotate::load(QString dir)
{
    QString fileName = dir + "/annotations.json";
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
                    a.text.setText(obj["text"].toString());
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
bool ClassAnnotate::save(QString dir)
{
    QString fileName = dir + "/annotations.json";
    qInfo() << "Saving annotations to" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
    {
        QJsonObject json;
        QJsonArray jsonArray;
        for (annotation &a : m_annot)
        {
            QJsonObject obj;
            obj["text"] = a.text.text();
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
