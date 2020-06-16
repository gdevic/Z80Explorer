#ifndef CLASSLOGIC_H
#define CLASSLOGIC_H

#include "AppTypes.h"
#include <QObject>

enum class LogicOp : unsigned char { Nop, Inverter, And, Nand, Or, Nor };

/*
 * This structure contains definition and code that describes a bipartite tree
 * of logic nodes for making the logic diagram of a net
 */
struct Logic
{
    net_t net;                          // Net number
    QString name;                       // Equivalent net name (or the net number as a string)
    bool leaf {};                       // True if this node is the leaf
    int tag {};                         // Used when rendering, don't depend on this value
    LogicOp op {LogicOp::Nop};          // Specifies the logic operation on its child nodes
    QVector<Logic *> children {};       // List of child nodes

    Logic() = delete;
    Logic(net_t n) : Logic(n, LogicOp::Nop) {}
    Logic(net_t n, LogicOp op);

    // Deletes the complete logic tree
    static void purge(Logic *ptr)
    {
        for (auto k : ptr->children)
            purge(k);
        delete ptr;
    }

    // Returns the name of a given logic operation as a string
    static inline QString toString(LogicOp op)
    {
        if (op == LogicOp::Inverter) return "INV";
        if (op == LogicOp::Nor) return "NOR";
        if (op == LogicOp::Nand) return "NAND";
        if (op == LogicOp::And) return "AND";
        return QString(); // LogicOp::Nop
    }

    // Prints the logic tree as a linear, flattened, equation
    static QString flatten(Logic *root)
    {
        QString eq = toString(root->op).isEmpty() ? root->name : toString(root->op);
        if (root->children.count())
        {
            eq.append('(');
            QStringList terms;
            for (auto k : root->children)
                terms.append(flatten(k));
            eq.append(terms.join(','));
            eq.append(')');
        }
        return eq;
    }
};

#endif // CLASSLOGIC_H
