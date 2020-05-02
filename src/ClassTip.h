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
    const QString get(net_t n)          // Returns a tip for a given net number
        { return m_tips.contains(n) ? m_tips.value(n) : QString(); }
    void set(const QString tip, net_t n)// Sets up the tip for a given net number
        { m_tips[n] = tip.trimmed(); }

    bool load(QString dir);             // Loads user tips
    bool save(QString dir);             // Saves user tips

public slots:
    void onShutdown();                  // Called when the app is closing

private:
    QMap<net_t, QString> m_tips;        // Map of nets to their tips; key is the net number
};

#endif // CLASSTIP_H
