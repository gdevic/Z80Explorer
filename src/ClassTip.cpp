#include "ClassTip.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

ClassTip::ClassTip(QObject *parent) : QObject(parent)
{
    m_tips[0] = "The net number zero should <b>never</> be used";
}

ClassTip::~ClassTip()
{
    QSettings settings;
    QString path = settings.value("ResourceDir").toString();
    Q_ASSERT(!path.isEmpty());
    save(path);
}

/*
 * Loads user tips from a file
 */
bool ClassTip::load(QString dir)
{
    QString fileName = dir + "/tips.json";
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

void ClassTip::read(const QJsonObject &json)
{
    if (json.contains("tips") && json["tips"].isArray())
    {
        QJsonArray array = json["tips"].toArray();
        m_tips.clear();

        for (int i = 0; i < array.size(); i++)
        {
            net_t net;
            QString tip;
            QJsonObject obj = array[i].toObject();
            if (obj.contains("net") && obj["net"].isDouble())
                net = obj["net"].toInt();
            if (obj.contains("tip") && obj["tip"].isString())
                tip = obj["tip"].toString();
            m_tips[net] = tip;
        }
    }
}

/*
 * Saves user tips to a file
 */
bool ClassTip::save(QString dir)
{
    QString fileName = dir + "/tips.json";
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
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

void ClassTip::write(QJsonObject &json) const
{
    QJsonArray jsonArray;
    QMapIterator<net_t, QString> i(m_tips);
    while (i.hasNext())
    {
        i.next();
        if (!i.value().trimmed().isEmpty())
        {
            QJsonObject obj;
            obj["net"] = i.key();
            obj["tip"] = i.value();
            jsonArray.append(obj);
        }
    }
    json["tips"] = jsonArray;
}
