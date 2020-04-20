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
    QFont font = painter.font();
    for (auto a : m_annot)
    {
        font.setPointSize(a.pts);
        painter.setFont(font);

        painter.drawText(a.pos, a.text);
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
                a.text = obj["text"].toString();
            if (obj.contains("x") && obj["x"].isDouble())
                a.pos.setX(obj["x"].toInt());
            if (obj.contains("y") && obj["y"].isDouble())
                a.pos.setY(obj["y"].toInt());
            if (obj.contains("pts") && obj["pts"].isDouble())
                a.pts = obj["pts"].toInt();
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
        obj["text"] = a.text;
        obj["x"] = a.pos.x();
        obj["y"] = a.pos.y();
        obj["pts"] = a.pts;
        jsonArray.append(obj);
    }
    json["annotations"] = jsonArray;
}
