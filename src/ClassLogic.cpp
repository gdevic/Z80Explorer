#include "ClassController.h"
#include "ClassLogic.h"
#include "ClassNetlist.h"
#include <algorithm>
#include <QSettings>

/*
 * Generates a logic equation for a net.
 * This code parses the netlist, starting at a given net, traversing upstream (by following the transistor paths that
 * gate eact net), and creates a tree of Logic nodes. It tries to figure out the logic function of each node (combination
 * of gates interconnected in various ways). It stops at the nets that are chosen as meaningful end points:
 * clk, t1..t6, m1..m6, flops and all PLA (instruction decode) signals. This list is customizable.
 */
// XXX Maybe we don't need to track visitedNets?
static QVector<net_t> visitedNets; // Avoid loops by keeping nets that are already visited
static QVector<tran_t> visitedTrans; // Avoid path duplication by keeping transistors that are already visited

Logic::Logic(net_t n, LogicOp op, bool checkVisitedNets) : outnet(n), op(op)
{
    QSettings settings;
    QString termNodes = settings.value("schematicTermNodes").toString();

    name = ::controller.getNetlist().get(n);
    if (name.isEmpty())
        name = QString::number(n);
    // We stop processing nodes at a leaf node which is either one of the predefined nodes or a detected loop
    leaf = n <= 3; // GND, VCC, CLK are always terminating
    leaf |= matchName(termNodes, name); // All other nets are user selectable
    if (checkVisitedNets && visitedNets.contains(n))
        leaf = true;
    else
        visitedNets.append(n);
}

Logic::~Logic() {}

/*
 * Optimizes, in place, logic tree by coalescing suitable nodes
 */
void ClassNetlist::optimizeLogicTree(Logic **ppl)
{
    QSettings settings;

    // Load most up to date optimization options
    optIntermediate = settings.value("schematicOptIntermediate").toBool();
    optInverters = settings.value("schematicOptInverters").toBool();
    optSingleInput = settings.value("schematicOptSingleInput").toBool();
    optClockGate = settings.value("schematicOptClockGate").toBool();
    optCoalesce = settings.value("schematicOptCoalesce").toBool();

    // Some optimizations may set up opportunity for additional optimizations, so we repeat the process until the
    // logic tree settles and there are no more changes to it.
    uint32_t lastSig, newSig = Logic::getLogicTreeSignature(*ppl);
    do
    {
        optimizeLinear(ppl);
        if (optCoalesce)
            optimizeAndOr(*ppl);
        if (optClockGate)
            optimizeClkNets(*ppl);

        qDebug() << "Logic tree optimization pass. Sig:" << newSig;
        lastSig = newSig;
        newSig = Logic::getLogicTreeSignature(*ppl);
    } while (newSig != lastSig);
}

/*
 * Recursive optimization of a logic net
 * Performs optimizations on logic symbols based around only one input
 */
void ClassNetlist::optimizeLinear(Logic **ppl)
{
    Logic *lr = *ppl;

    // Exit if there are no inputs to process (this also handles leaf nodes / terminating nodes)
    if (lr->inputs.isEmpty())
        return;

    // We know the current node has at least one input net
    Logic *next = lr->inputs[0];

    // If the node is "AND" or "OR" with only a single input, remove it
    if (optSingleInput && ((lr->op == LogicOp::And) || (lr->op == LogicOp::Or)) && (lr->inputs.size() == 1))
    {
        *ppl = lr->inputs[0]; // Bypass the current node
        delete lr;
        return optimizeLinear(ppl);
    }
    // If the node is "NAND" or "NOR" with only a single input, replace it with an inverter
    if (optSingleInput && ((lr->op == LogicOp::Nand) || (lr->op == LogicOp::Nor)) && (lr->inputs.size() == 1))
    {
        lr->op = LogicOp::Inverter;
        return optimizeLinear(ppl);
    }
    // Coalesce an inverter with And/Or into Nand/Nor
    if (optInverters && (lr->op == LogicOp::Inverter) && ((next->op == LogicOp::And) || (next->op == LogicOp::Or)))
    {
        *ppl = next; // Bypass the inverter
        next->op = (next->op == LogicOp::And) ? LogicOp::Nand : LogicOp::Nor;
        delete lr;
        return optimizeLinear(ppl);
    }
    // Coalesce an inverter with Nand/Nor into And/Or
    if (optInverters && (lr->op == LogicOp::Inverter) && ((next->op == LogicOp::Nand) || (next->op == LogicOp::Nor)))
    {
        *ppl = next; // Bypass the inverter
        next->op = (next->op == LogicOp::Nand) ? LogicOp::And : LogicOp::Or;
        delete lr;
        return optimizeLinear(ppl);
    }
    // If the node is an inverter and its input is also an inverter, remove both
    if (optInverters && (lr->op == LogicOp::Inverter) && (next->op == LogicOp::Inverter) && !next->inputs.isEmpty())
    {
        *ppl = next->inputs[0]; // Bypass both inverters
        delete next;
        delete lr;
        return optimizeLinear(ppl);
    }
    // Remove intermediate nets
    if (optIntermediate && !lr->root && (lr->op == LogicOp::Net))
    {
        *ppl = next; // Bypass the net
        delete lr;
        return optimizeLinear(ppl);
    }
    // Remove clock gates
    if (optClockGate && (lr->op == LogicOp::ClkGate))
    {
        *ppl = next; // Bypass the current node (a clock gate)
        delete lr;
        return optimizeLinear(ppl);
    }

    for (int i = 0; i < lr->inputs.count(); i++)
        optimizeLinear(&lr->inputs[i]);
}

/*
 * Recursive optimization of a logic net
 * Performs a more complex optimization of merging (coalescing) identical, successive AND/OR gates
 */
void ClassNetlist::optimizeAndOr(Logic *p)
{
    if ((p->op == LogicOp::And) || (p->op == LogicOp::Or))
    {
        // Loop over a copy of inputs since we will be removing selected entries
        QVector<Logic *> children = p->inputs;
        QVector<Logic *> grandchildren;

        for (auto *lp : children)
        {
            if (lp->op == p->op)
            {
                grandchildren.append(lp->inputs);
                p->inputs.removeOne(lp);
                delete lp;
            }
        }
        p->inputs.append(grandchildren);
    }

    for (auto *lp : p->inputs)
        optimizeAndOr(lp);
}

/*
 * Recursive optimization of a logic net
 * Optimize by removing clock inputs
 */
void ClassNetlist::optimizeClkNets(Logic *p)
{
    // Find the index (if any) of the first CLK net in the inputs and delete it
    int clkIndex = -1;
    for (int i = 0; i < p->inputs.count(); i++)
    {
        if (p->inputs[i]->outnet == nclk)
        {
            clkIndex = i;
            break;
        }
    }
    if (clkIndex >= 0)
    {
        delete p->inputs[clkIndex];
        p->inputs.removeAt(clkIndex);
    }

    for (auto *lp : p->inputs)
        optimizeClkNets(lp);
}

Logic *ClassNetlist::getLogicTree(net_t net)
{
    QSettings settings;
    auto maxDepth = settings.value("schematicMaxDepth").toInt();

    visitedNets.clear();
    visitedTrans.clear();

    auto root = new Logic(net, LogicOp::Net);
    root->root = true;
    parse(root, maxDepth);

    return root;
}

/*
 * Returns a string describing the logic connections of a net
 */
QString ClassNetlist::equation(net_t net)
{
    Logic *lr = getLogicTree(net);
    QString equation = lr->name % " = " % Logic::flatten(lr);
    Logic::purge(lr);
    qDebug() << equation;
    return equation;
}

// Returns indices of transistors that share the same c2 value (2 or more occurrences)
QVector<int> findSharedC2Indices(const QVector<Trans> &trans)
{
    QVector<int> result;

    // Vector must have at least 2 elements to have any sharing
    if (trans.size() < 2)
        return result;

    // Since the vector is already sorted by c2, we can use a sliding window approach
    int startIdx = 0;
    net_t currentC2 = trans[0].c2;
    int count = 1;

    // Scan through the sorted vector
    for (int i = 1; i < trans.size(); ++i)
    {
        if (trans[i].c2 == currentC2) // Found another transistor with same c2
            count++;
        else // Different c2 found
        {
            if (count >= 2)
            {
                for (int j = startIdx; j < i; ++j) // Add indices of all transistors that shared the previous c2
                    result.append(j);
            }
            // Start new group
            startIdx = i;
            currentC2 = trans[i].c2;
            count = 1;
        }
    }
    // Handle the last group
    if (count >= 2)
    {
        for (int j = startIdx; j < trans.size(); ++j)
            result.append(j);
    }
    return result;
}

Logic *ClassNetlist::parse(Logic *node, int depth)
{
    if (node->leaf)
        return node;
    // If we have reached the terminal graph depth, issue a dotdot symbol and exit
    if (!--depth)
    {
        Logic *next = new Logic(node->outnet, LogicOp::DotDot);
        node->inputs.append(next);
        return next;
    }

    net_t net0id = node->outnet;
    Net &net0 = m_netlist[net0id];

    // Copy the list of transistors for which this net is either a source or a drain, but skip over transistors already visited
    // While copying, make sure c1 contains our primary net number, and c2 is "the other end" net
    node->trans.reserve(net0.c1c2s.size());
    for (const Trans *t : net0.c1c2s)
    {
        if (!visitedTrans.contains(t->id)) // Skip over transistors already visited
        {
            Trans newTrans = *t; // Make a copy of the transistor

            if (t->c1 != net0id) // If c1 is not net0id, swap c1 and c2 to make net0id always c1
                std::swap(newTrans.c1, newTrans.c2);

            node->trans.emplace_back(newTrans);
        }
    }

    // Sort the transistors by their c2 value ("the other end")
    std::sort(node->trans.begin(), node->trans.end(), [](const Trans &a, const Trans &b) { return a.c2 < b.c2; });

    // Recognize simple single gate patterns:
    //-------------------------------------------------------------------------------
    // Detect and handle a clock gate
    //-------------------------------------------------------------------------------
    if ((node->trans.count() == 1) && (node->trans[0].gate == nclk))
    {
        qDebug() << "Clock gate t=" << node->trans[0].id;
        visitedTrans.append(node->trans[0].id);
        net_t net_other = node->trans[0].c2; // The net on the other side of the transistor
        Logic *next = new Logic(net_other, LogicOp::ClkGate, false);
        node->inputs.append(next);
        return parse(next, depth);
    }
    //-------------------------------------------------------------------------------
    // Detect and handle an inverter
    //-------------------------------------------------------------------------------
    if ((node->trans.count() == 1) && (node->trans[0].c2 == ngnd))
    {
        qDebug() << "Inverter t=" << node->trans[0].id;
        visitedTrans.append(node->trans[0].id);
        net_t net_other = node->trans[0].gate; // The net being inverted
        Logic *next = new Logic(net_other, LogicOp::Inverter, false);
        node->inputs.append(next);
        return parse(next, depth);
    }
    //-------------------------------------------------------------------------------
    // Detect and handle a clocked push/pull inverter driver
    //-------------------------------------------------------------------------------
    if (!net0.hasPullup && (node->trans.count() == 3) && (node->trans[0].c2 == ngnd) && (node->trans[1].c2 == ngnd) && (node->trans[2].c2 == npwr))
    {
        net_t aux_net = node->trans[2].gate;
        Net &netaux = m_netlist[aux_net];
        if (netaux.hasPullup && (netaux.c1c2s.count() == 2) && (netaux.c1c2s[0]->c2 == ngnd) && (netaux.c1c2s[1]->c2 == ngnd))
        {
            qDebug() << "Clocked push/pull inverter driver t=" << node->trans[0].id;
            visitedTrans.append(netaux.c1c2s[0]->id);
            visitedTrans.append(netaux.c1c2s[1]->id);
            visitedTrans.append(node->trans[0].id);
            visitedTrans.append(node->trans[1].id);
            net_t net_other = (node->trans[0].gate == nclk) ? node->trans[1].gate : node->trans[0].gate;
            Logic *aux = new Logic(aux_net, LogicOp::Inverter, false);
            node->inputs.append(aux);
            Logic *next = new Logic(net_other, LogicOp::ClkGate, false);
            next->name = "~"; // Signal drawing of an inverted clock gate
            aux->inputs.append(next);
            return parse(next, depth);
        }
    }
    //-------------------------------------------------------------------------------
    // Detect and handle a push/pull inverter driver
    //-------------------------------------------------------------------------------
    if (!net0.hasPullup && ((node->trans.count() >= 2) && (node->trans[0].c2 == ngnd) && (node->trans[1].c2 == npwr)))
    {
        qDebug() << "Push/pull inverter driver t=" << node->trans[0].id;
        // A driving net is the one connecting one of the transistors to the ground
        net_t net_other = (node->trans[0].c2 == ngnd) ? node->trans[0].gate : node->trans[1].gate;
        visitedTrans.append(node->trans[0].id);
        Logic *next = new Logic(net_other, LogicOp::Inverter, false);
        next->name = "P/P";
        node->inputs.append(next);
        return parse(next, depth);
    }

    // Will our heuristic fail if the net is not pulled up?
    if (!net0.hasPullup)
        qDebug() << "Net" << net0id << "is not pulled up.";

    if (node->trans.isEmpty())
    {
        qDebug() << "Aborting an unexpected path via net" << net0id;
        return node;
    }

    // The following cases form inputs to a NOR logic gate
    // If we end up with only one input, we will change it to a benign net type
    Logic *root = new Logic(net0id, LogicOp::Nor, false);
    root->trans = node->trans;
    node->inputs.append(root);

    auto safety = 8; // Safety counter to avoid infinite loops
    while (!root->trans.isEmpty() && --safety)
    {
        // Get indices of transistors that share c2 values
        QVector<int> shared = findSharedC2Indices(root->trans);

        //-------------------------------------------------------------------------------
        // Detect two or more, parallel input, OR gates
        //-------------------------------------------------------------------------------
        if (!shared.isEmpty())
        {
            net_t net_other = root->trans[shared[0]].c2; // The net on the other side of that transistor (a shared net)

            // If the common net was not GND, it requires to be combined in with a NOR gate (ex. net 509)
            if (net_other != ngnd)
            {
                Logic *next = new Logic(net_other, LogicOp::Or, false);
                root->inputs.append(next);

                Logic *nor = new Logic(net_other, LogicOp::Nor, false);

                // Contributing nets form a NOR gate
                for (auto i : shared)
                {
                    visitedTrans.append(root->trans[i].id);

                    net_t net_gate = root->trans[i].gate;
                    Logic *next = new Logic(net_gate);
                    nor->inputs.append(next);
                    parse(next, depth);
                }

                next->inputs.append(nor);
                parse(next, depth);
            }
            else
            {
                // Contributing nets form an OR gate
                for (auto i : shared)
                {
                    visitedTrans.append(root->trans[i].id);

                    net_t net_gate = root->trans[i].gate;
                    Logic *next = new Logic(net_gate);
                    root->inputs.append(next);
                    parse(next, depth);
                }
            }

            // Remove all transistors that have the same c2 as net_other
            root->trans.erase(
                std::remove_if(root->trans.begin(), root->trans.end(),
                    [net_other](const Trans &t) { return t.c2 == net_other; }),
                root->trans.end()
            );

            if (root->trans.isEmpty())
                qDebug() << "Path1 trans not empty" << net0id;
        }
        else // If there are no shared "other side" nets
        {
            Trans &t1 = root->trans[0];

            //-------------------------------------------------------------------------------
            // Detect if a net is a part of a latch
            //-------------------------------------------------------------------------------
            latchdef *latch = ::controller.getChip().getLatch(t1.id);
            if (latch != nullptr)
            {
                visitedTrans.append(latch->t1);
                visitedTrans.append(latch->t2);

                Logic *next = new Logic(net0id, LogicOp::Latch, false);
                next->leaf = true;
                next->name = latch->name;
                root->inputs.append(next);

                // Remove first transistor
                root->trans.removeFirst();
                continue;
            }

            //-------------------------------------------------------------------------------
            // Complex NAND gate extends through 2 pass-transistor nets (ex. net 215)
            //-------------------------------------------------------------------------------
            net_t net1gt = t1.gate;
            net_t net1id = t1.c2;
            Net &net1 = m_netlist[net1id];

            // NAND gate extending for 2 pass-transistor nets (ex. net 215)
            if ((net1.gates.count() == 0) && (net1.c1c2s.count() == 2) && !net1.hasPullup)
            {
                Trans *t2 = (net1.c1c2s[0]->id == t1.id) ? net1.c1c2s[1] : net1.c1c2s[0]; // Second transistor in the chain
                net_t net2gt = t2->gate;
                net_t net2id = t2->c2;
                Net &net2 = m_netlist[net2id];

                // If the second transistor's c2 is GND, we have a 2-input NAND gate (ex. net 215)
                if (net2id == ngnd)
                {
                    Logic *next = new Logic(net1id, LogicOp::Nand);
                    root->inputs.append(next);

                    Logic *next_and1 = new Logic(net1gt);
                    next->inputs.append(next_and1);
                    visitedTrans.append(root->trans[0].id);
                    parse(next_and1, depth);

                    Logic *next_and2 = new Logic(net2gt);
                    next->inputs.append(next_and2);
                    visitedTrans.append(t2->id);
                    parse(next_and2, depth);

                    // Remove first transistor
                    root->trans.removeFirst();
                    continue;
                }
                //-------------------------------------------------------------------------------
                // Complex NAND gate extends through 3 pass-transistor nets (ex. net 235)
                //-------------------------------------------------------------------------------
                if ((net2.gates.count() == 0) && (net2.c1c2s.count() == 2) && !net2.hasPullup)
                {
                    Trans *t3 = (net2.c1c2s[0]->id == t2->id) ? net2.c1c2s[1] : net2.c1c2s[0];
                    net_t net3gt = t3->gate;
                    net_t net3id = t3->c2;

                    // If the third transistor's c2 is GND, we have a 3-input NAND gate (ex. net 235)
                    if (net3id == ngnd)
                    {
                        Logic *next = new Logic(net1id, LogicOp::Nand);
                        root->inputs.append(next);

                        Logic *next_and1 = new Logic(net1gt);
                        next->inputs.append(next_and1);
                        visitedTrans.append(root->trans[0].id);
                        parse(next_and1, depth);

                        Logic *next_and2 = new Logic(net2gt);
                        next->inputs.append(next_and2);
                        visitedTrans.append(t2->id);
                        parse(next_and2, depth);

                        Logic *next_and3 = new Logic(net3gt);
                        next->inputs.append(next_and3);
                        visitedTrans.append(t3->id);
                        parse(next_and3, depth);

                        // Remove first transistor
                        root->trans.removeFirst();
                        continue;
                    }
                }
            }
            // Detect and handle inverter
            if (root->trans[0].c2 == ngnd)
            {
                qDebug() << "Inverter t=" << root->trans[0].id;
                visitedTrans.append(root->trans[0].id);
                net_t net_other = root->trans[0].gate; // The net being inverted
                Logic *next = new Logic(net_other, LogicOp::Inverter, false);
                root->inputs.append(next);
                parse(next, depth);

                // Remove first transistor
                root->trans.removeFirst();
                continue;
            }
            // Detect and handle single transistor pass gate: AND with an inverter on the source (but not on the gate) net
            {
                qDebug() << "Single transistor pass gate t=" << root->trans[0].id;
                visitedTrans.append(root->trans[0].id);
                net_t net_other = root->trans[0].c2; // The net on the other side of that transistor
                net_t net_gate = root->trans[0].gate; // The net connected to the gate

                Logic *next_nand = new Logic(root->outnet, LogicOp::And, false);
                root->inputs.append(next_nand);

                Logic *next_inv = new Logic(net_other, LogicOp::Inverter);
                next_nand->inputs.append(next_inv);
                parse(next_inv, depth);

                Logic *next_gate = new Logic(net_gate);
                next_nand->inputs.append(next_gate);
                parse(next_gate, depth);

                // Remove first transistor
                root->trans.removeFirst();
                continue;
            }
        }
    }
    // If the node has only one input, change it to a benign net type
    if (root->inputs.count() == 1)
        root->op = LogicOp::Net;
    if (!safety)
        qWarning() << "parse" << net0id << "safety loop count reached!";
    return node;
}
