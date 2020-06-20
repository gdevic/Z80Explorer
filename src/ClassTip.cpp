#include "ClassTip.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ClassTip::ClassTip(QObject *parent) : QObject(parent)
{
}

void ClassTip::onShutdown()
{
    save(m_jsonFile);
}

/*
 * Loads user tips from a file
 */
bool ClassTip::load(QString fileName)
{
    if (m_jsonFile.isEmpty()) // Set the initial file name
        m_jsonFile = fileName;
    qInfo() << "Loading custom tips from" << fileName;
    QFile loadFile(fileName);
    if (loadFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(data));
        const QJsonObject json = loadDoc.object();

        if (json.contains("tips") && json["tips"].isArray())
        {
            QJsonArray array = json["tips"].toArray();
            m_tips.clear();

            for (int i = 0; i < array.size(); i++)
            {
                net_t net = 0;
                QString tip;
                QJsonObject obj = array[i].toObject();
                if (obj.contains("net") && obj["net"].isDouble())
                    net = obj["net"].toInt();
                if (obj.contains("tip") && obj["tip"].isString())
                    tip = obj["tip"].toString();
                if (net)
                    m_tips[net] = tip;
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
 * Saves user tips to a file
 */
bool ClassTip::save(QString fileName)
{
    qInfo() << "Saving custom tips to" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
    {
        QJsonObject json;
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

        QJsonDocument saveDoc(json);
        saveFile.write(saveDoc.toJson());
        return true;
    }
    else
        qWarning() << "Unable to save" << fileName;
    return false;
}
