#ifndef CLASSLOGIC_H
#define CLASSLOGIC_H

#include "AppTypes.h"
#include <QObject>

struct Trans;

// Logic functions ("Net" and "DotDot" have no logic function but are connectors, nets)
enum class LogicOp : unsigned char { Net, Inverter, And, Nand, Or, Nor, ClkGate, Latch, DotDot };

/*
 * This structure contains definition and code that describes a tree
 * of logic nodes for making the logic diagram of the net flow
 */
struct Logic
{
public:
    // Define the logic gate represented by this struct
    LogicOp op;                         // Specifies the logic operation on its input nodes

    // Lists the inputs to this logic gate
    QVector<Logic *> inputs;            // List of input nodes

    // Names the output of this logic gate, this is always a net
    net_t outnet;                       // The output net number, or the net itself (for LogicOp::Net)
    QString name;                       // The net name (or the net number as a string)

    // Misc members
    bool leaf {};                       // True if this node is a leaf (a terminating node); also, it has no inputs
    bool root {};                       // True if this node is the root node (will not optimize away)
    QVector<Trans> trans {};            // A copy of transistors for which this net is either a source or a drain
    int tag {};                         // Temporary internal variable

public:
    Logic(net_t n, LogicOp op = LogicOp::Net, bool checkVisitedNets = true);
    Logic() = delete;
    ~Logic();

    // Deletes the logic tree
    static void purge(Logic *ptr)
    {
        qDeleteAll(ptr->inputs);
        delete ptr;
    }

    // Returns the name of a given logic operation as a string
    static const QString toString(LogicOp op)
    {
        static const QStringList ops {"", "INV", "AND", "NAND", "OR", "NOR", "CLKGATE", "LATCH", "..."};
        return ops[static_cast<int>(op)];
    }

    // Prints the logic tree as a linear, flattened, equation
    static QString flatten(Logic *root)
    {
        QString base = toString(root->op).isEmpty() ? root->name : toString(root->op);
        if (root->inputs.count())
        {
            QStringList terms;
            for (auto k : root->inputs)
                terms.append(flatten(k));
            return base % QLatin1Char('(') % terms.join(',') % QLatin1Char(')');
        }
        return base;
    }
};

#endif // CLASSLOGIC_H
