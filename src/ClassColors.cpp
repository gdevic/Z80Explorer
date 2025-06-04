#include "ClassColors.h"
#include "ClassController.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

ClassColors::ClassColors(QObject *parent) : QObject(parent)
{}

void ClassColors::onShutdown()
{
    save(m_jsonFile);
}

/*
 * Updates internal color table (implemented as a hash) based on the colors specifications
 * There are 4 methods that each net/bus can be matched to each coloring defintion:
 * 0 .. Name has to match exactly the string in the definition
 * 1 .. Name only has to start with the string in the definition (and may be longer)
 * 2 .. Definition string is a regular expression trying to match each name
 * 3 .. Definition string is an explicit net number to color (a pure decimal value like "42")
 */
void ClassColors::rebuild()
{
    qInfo() << "Updating coloring table";
    m_colors.clear();
    // Pre-populate defaults for a few important nets:
    m_colors[0] = getInactive();
    m_colors[1] = getVss();
    m_colors[2] = getVcc();

    QRegularExpression re;
    QStringList netNames = ::controller.getNetlist().getNetnames();

    for (auto &colordef : m_colordefs)
    {
        if (!colordef.enabled) // Ignore disabled color specifications
            continue;
        re.setPattern(colordef.expr);
        // Compare each colordef with all known (named) nets for the matching methods 0, 1 and 2
        for (auto name : netNames)
        {
            bool matching = false;
            if (colordef.method == 0)       // Method 0: name has to match exactly as specified
                matching = colordef.expr == name;
            else if (colordef.method == 1)  // Method 1: only the name start needs to match to the specified string
                matching = name.startsWith(colordef.expr);
            else if (colordef.method == 2)  // Method 2: uses regular expression to find a match
            {
                QRegularExpressionMatch match = re.match(name);
                matching = match.hasMatch();
            }
            else if (colordef.method == 3)  // Method 3: specified string is an explicit net number to match
            {} // Will do later, below
            else
            {
                qWarning() << "Invalid coloring method" << colordef.method << "for net" << name;
                return;
            }

            if (matching)
            {
                net_t net = ::controller.getNetlist().get(name);
                if (net)
                {
                    // Remove any previously defined (duplicate) color; keep the last one
                    if (m_colors.contains(net))
                        m_colors.remove(net);
                    m_colors[net] = colordef.color;
                }
                else
                {
                    const QVector<net_t> &bus = ::controller.getNetlist().getBus(name);
                    for (auto n : bus)
                    {
                        // Remove any previously defined (duplicate) color; keep the last one
                        if (m_colors.contains(n))
                            m_colors.remove(n);
                        m_colors[n] = colordef.color;
                    }
                }
            }
        }
        // Directly assign nets for all colordefs with matching method 3
        if (colordef.method == 3)
        {
            bool ok;
            net_t net = colordef.expr.toUInt(&ok);
            if (ok)
            {
                // Remove any previously defined (duplicate) color; keep the last one
                if (m_colors.contains(net))
                    m_colors.remove(net);
                m_colors[net] = colordef.color;
            }
        }
    }
}

/*
 * Sets a new colordefs array, used by the colors editor dialog
 */
void ClassColors::setColordefs(QVector<colordef> colordefs)
{
    m_colordefs = colordefs;
    rebuild(); // Update internal color table
    // Sending this signal will cause ClassVisual to redraw its colorized image
    emit ::controller.eventNetName(Netop::Changed, QString(), 0);
}

/*
 * Loads color definitions from a file
 */
bool ClassColors::load(QString fileName, bool merge)
{
    if (m_jsonFile.isEmpty()) // Set the initial file name
        m_jsonFile = fileName;
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
            if (!merge)
                m_colordefs.clear();

            for (int i = 0; i < array.size(); i++)
            {
                colordef c{};
                QJsonObject obj = array[i].toObject();
                if (obj.contains("expr") && obj["expr"].isString())
                    c.expr = obj["expr"].toString();
                if (obj.contains("method") && obj["method"].isDouble())
                    c.method = obj["method"].toDouble();
                if (obj.contains("color") && obj["color"].isString())
                {
                    QStringList s = obj["color"].toString().split(',');
                    if (s.count() == 4)
                        c.color = QColor(s[0].toInt(), s[1].toInt(), s[2].toUInt(), s[3].toInt());
                }
                if (obj.contains("enabled") && obj["enabled"].isBool())
                    c.enabled = obj["enabled"].toBool();
                m_colordefs.append(c);
            }
            rebuild();
            if (!merge) // Do not update file name if merging
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
 * Saves color definitions to a file
 */
bool ClassColors::save(QString fileName)
{
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
            obj["method"] = int(c.method);
            obj["color"] = QString("%1,%2,%3,%4").arg(c.color.red()).arg(c.color.green()).arg(c.color.blue()).arg(c.color.alpha());
            obj["enabled"] = c.enabled;
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
