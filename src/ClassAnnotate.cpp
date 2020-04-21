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

bool ClassAnnotate::init()
{
    m_fixedFont = QFont("Consolas");

    return true;
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
//    qDebug() << imageView << scale;

    painter.setPen(Qt::white);
    for (auto a : m_annot)
    {
        painter.save(); // Save and restore painter state since each text has its own translation/size
        m_fixedFont.setPixelSize(a.pts); // Base all text on the same font family; set the size
        painter.setFont(m_fixedFont);
        QSizeF size = a.text.size();
        painter.translate(a.pos); // Read this sequence in the reverse order... Finally, translate to the required image coordinates
        painter.rotate(a.angle); // Rotate text by a angle
        painter.translate(0, -size.height()); // Translate up by the text height so the anchor is at the bottom left
        painter.drawStaticText(0, 0, a.text);
        size = a.text.size();
        painter.restore();
    }
}

ClassAnnotate::~ClassAnnotate()
{
    QSettings settings;
    QString path = settings.value("ResourceDir").toString();
    Q_ASSERT(!path.isEmpty());
    save(path);
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
            if (obj.contains("pts") && obj["pts"].isDouble())
                a.pts = obj["pts"].toInt();
            if (obj.contains("angle") && obj["angle"].isDouble())
                a.angle = obj["angle"].toInt();
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
        obj["pts"] = int(a.pts);
        obj["angle"] = a.angle;
        jsonArray.append(obj);
    }
    json["annotations"] = jsonArray;
}
