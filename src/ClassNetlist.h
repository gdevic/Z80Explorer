#ifndef CLASSNETLIST_H
#define CLASSNETLIST_H

#include "z80state.h"
#include <QObject>
#include <QHash>

#define MAX_NET 3600 // Max number of nets

/*
 * This class represents netlist data
 */
class ClassNetlist
{
public:
    ClassNetlist();

    bool loadResources(QString dir);

private:
    bool loadNetNames(QString dir);

private:
    // The lookup between net names and their numbers is performance critical, so we keep two ways to access them:
    QString m_netnames[MAX_NET];            // List of net names, directly indexed by the net number
    QHash<QString, net_t> m_netnums;        // Hash of net names to their net numbers; key is the net name string
};

#endif // CLASSNETLIST_H
