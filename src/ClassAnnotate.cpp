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
            Annotation a;
            QJsonObject obj = array[i].toObject();
            if (obj.contains("text") && obj["text"].isString())
                a.text = obj["text"].toString();
            if (obj.contains("x") && obj["x"].isDouble())
                a.pos.setX(obj["x"].toInt());
            if (obj.contains("y") && obj["y"].isDouble())
                a.pos.setY(obj["y"].toInt());
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
    for (const Annotation &a : m_annot)
    {
        QJsonObject obj;
        obj["text"] = a.text;
        obj["x"] = a.pos.x();
        obj["y"] = a.pos.y();
        jsonArray.append(obj);
    }
    json["annotations"] = jsonArray;
}
