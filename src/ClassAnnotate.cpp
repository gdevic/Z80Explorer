#include "ClassAnnotate.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

ClassAnnotate::ClassAnnotate(QObject *parent) : QObject(parent)
{
}

ClassAnnotate::~ClassAnnotate()
{
    QSettings settings;
    QString path = settings.value("ResourceDir").toString();
    Q_ASSERT(!path.isEmpty());
    save(path);
}

bool ClassAnnotate::init()
{
    m_fixedFont = QFont("Consolas");

    return true;
}

/*
 * Adds annotation to the list
 */
void ClassAnnotate::add(QString text, QRect box)
{
    annotation a(text);

    const qreal someXFactor = 1.8; // Depending on a font, we need to stretch its rendering
    qreal pixX = qreal(box.width()) / (text.length() / someXFactor);
    qreal pixY = box.height();
    qreal pix = qMin(pixX, pixY);

    a.pix = pix;
    a.pos = box.topLeft();

    if (pixX < pixY) // Center the text vertically
        a.pos += QPoint(0, (pixY - pixX) / 2);

    m_annot.append(a);
}

/*
 * Draws annotations
 * imageView defines the current viewport in the image space, used to clip
 * scale is the current image scaling value, used to selectively draw different text sizes
 */
void ClassAnnotate::draw(QPainter &painter, QRectF imageView, qreal scale)
{
    Q_UNUSED(imageView);
    Q_UNUSED(scale);

    painter.setPen(Qt::white);
    for (auto a : m_annot)
    {
        // Selective rendering hides annotations that are too large or too small for the given scale
        qreal apparent = a.pix * scale;
        if (apparent > 200 || apparent < 8)
            continue;

        // drawStaticText() anchor is at the top-left point (drawText() is on the bottom-left)
        m_fixedFont.setPixelSize(a.pix); // Base all text on the same font family; set the size
        painter.setFont(m_fixedFont);
        painter.drawStaticText(a.pos, a.text);
    }
}

/*
 * Loads user annotations from a file
 */
bool ClassAnnotate::load(QString dir)
{
    QString fileName = dir + "/annotations.json";
    QFile loadFile(fileName);
    if (loadFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(data));
        read(loadDoc.object());

        return true;
    }
    else
        qWarning() << "Unable to load" << fileName;
    return false;
}

void ClassAnnotate::read(const QJsonObject &json)
{
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
            if (obj.contains("pix") && obj["pix"].isDouble())
                a.pix = obj["pix"].toInt();
            m_annot.append(a);
        }
    }
}

/*
 * Saves user annotations to a file
 */
bool ClassAnnotate::save(QString dir)
{
    QString fileName = dir + "/annotations.json";
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly))
    {
        QJsonObject data;
        write(data);
        QJsonDocument saveDoc(data);
        saveFile.write(saveDoc.toJson());

        return true;
    }
    else
        qWarning() << "Unable to save" << fileName;
    return false;
}

void ClassAnnotate::write(QJsonObject &json) const
{
    QJsonArray jsonArray;
    for (const annotation &a : m_annot)
    {
        QJsonObject obj;
        obj["text"] = a.text.text();
        obj["x"] = a.pos.x();
        obj["y"] = a.pos.y();
        obj["pix"] = int(a.pix);
        jsonArray.append(obj);
    }
    json["annotations"] = jsonArray;
}
