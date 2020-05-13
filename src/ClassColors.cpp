#include "ClassColors.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

ClassColors::ClassColors(QObject *parent) : QObject(parent)
{
    // Populate some known colors
    m_colors[0] = QColor(Qt::black);        // Also used as an invalid color
    m_colors[1] = QColor(  0, 127,   0);    // vss medium green
    m_colors[2] = QColor(192,   0,   0);    // vcc quite red
    m_colors[3] = QColor(192,192,192);      // clk subdued white

    m_colordefs.append({"clk", QColor(200, 200, 200)});
    m_colordefs.append({"m", QColor(128, 192, 128)});
    m_colordefs.append({"t", QColor(128, 128, 192)});
    m_colordefs.append({"pla", QColor(128, 192, 192)});
    m_colordefs.append({"dbus", QColor(0, 192, 0)});
    m_colordefs.append({"ubus", QColor(128, 255, 0)});
    m_colordefs.append({"vbus", QColor(0, 255, 128)});
    m_colordefs.append({"abus", QColor(128, 128, 255)});
}

void ClassColors::onShutdown()
{
    QSettings settings;
    QString path = settings.value("ResourceDir").toString();
    Q_ASSERT(!path.isEmpty());
    if (m_colordefs.size()) // // Save the color definitions only if we have any
        save(path);
}

/*
 * Loads color definitions from a file
 */
bool ClassColors::load(QString dir)
{
    QString fileName = dir + "/colors.json";
    qInfo() << "Loading color definitions from" << fileName;
    QFile loadFile(fileName);
    if (loadFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(data));
        const QJsonObject json = loadDoc.object();

        if (json.contains("colors") && json["colors"].isArray())
        {
            QJsonArray array = json["colors"].toArray();
            m_colordefs.clear();
            m_colordefs.reserve(array.size());

            for (int i = 0; i < array.size(); i++)
            {
                colordef c;
                QJsonObject obj = array[i].toObject();
                if (obj.contains("expr") && obj["expr"].isString())
                    c.expr = obj["expr"].toString();
                if (obj.contains("color") && obj["color"].isString())
                {
                    QStringList s = obj["color"].toString().split(',');
                    if (s.count() == 4)
                        c.color = QColor(s[0].toInt(), s[1].toInt(), s[2].toUInt(), s[3].toInt());
                }
                m_colordefs.append(c);
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
 * Saves color definitions to a file
 */
bool ClassColors::save(QString dir)
{
    QString fileName = dir + "/colors.json";
    qInfo() << "Saving color definitions to" << fileName;
    QFile saveFile(fileName);
    if (saveFile.open(QIODevice::WriteOnly | QFile::Text))
    {
        QJsonObject json;
        QJsonArray jsonArray;
        for (auto &c : m_colordefs)
        {
            QJsonObject obj;
            obj["expr"] = c.expr;
            obj["color"] = QString("%1,%2,%3,%4").arg(c.color.red()).arg(c.color.green()).arg(c.color.blue()).arg(c.color.alpha());
            jsonArray.append(obj);
        }
        json["colors"] = jsonArray;

        QJsonDocument saveDoc(json);
        saveFile.write(saveDoc.toJson());
        return true;
    }
    else
        qWarning() << "Unable to save" << fileName;
    return false;
}
