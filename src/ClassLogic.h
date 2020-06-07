#ifndef CLASSLOGIC_H
#define CLASSLOGIC_H

#include <QObject>
#include "AppTypes.h"

enum class LogicOp : unsigned char { Nop, Inverter, Nand, Nor, And };

// Contains a node in a logic bipartite tree
struct Logic
{
    net_t net;                          // Net number
    QString name;                       // Equivalent net name (or the net number as a string)
    bool leaf {};                       // True if this node is the leaf
    LogicOp op {LogicOp::Nop};          // Specifies the logic operation on children
    QVector<Logic *> children {};       // Pointers to the child nodes

    Logic() = delete;
    Logic(net_t n) : Logic(n, LogicOp::Nop) {}
    Logic(net_t n, LogicOp op);
};


#endif // CLASSLOGIC_H
