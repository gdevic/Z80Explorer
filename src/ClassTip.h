#ifndef CLASSTIP_H
#define CLASSTIP_H

#include "AppTypes.h"
#include <QObject>
#include <QMap>

/*
 * This class contains the code to support signal tips which are short custom descriptions
 * of each signal's function or purpose.
 */
class ClassTip : public QObject
{
    Q_OBJECT
public:
    explicit ClassTip(QObject *parent = nullptr);
    ~ClassTip();
    const QString get(net_t n)          // Returns a tip for a given net number
        { return m_tips.contains(n) ? m_tips.value(n) : QString(); }
    void set(const QString tip, net_t n)// Sets the tip for a given net number
        { m_tips[n] = tip.trimmed(); }

    bool load(QString dir);             // Loads user tips
    bool save(QString dir);             // Saves user tips

private:
    QMap<net_t, QString> m_tips;        // Map of tips to their description; key is the net number

    void read(const QJsonObject &json);
    void write(QJsonObject &json) const;

};

#endif // CLASSTIP_H