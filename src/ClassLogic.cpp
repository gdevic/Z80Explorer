#include "ClassLogic.h"
#include "ClassController.h"
#include "ClassNetlist.h"

/*
 * Generates a logic equation for a net
 * Parses the netlist, starting at the given node, and creates a logic equation
 * that includes all the nets driving it. The tree stops with the nets that are
 * chosen as meaningful end points: clk, t1..t6, m1..m6 and the PLA signals.
 */
QVector<net_t> visitedNets; // Avoid loops by keeping nets that are already visited
QVector<tran_t> visitedTran; // Avoid path duplication by keeping transistors that are already visited

Logic *ClassNetlist::getLogicTree(net_t net)
{
    visitedNets.clear();
    visitedTran.clear();
    Logic *root = new Logic(net);
    parse(root);
    // Fixup for the root node which might have been reused for a NOR gate
    if (root->op != LogicOp::Nop)
    {
        Logic *root2 = new Logic(net);
        root2->children.append(root);
        return root2;
    }
    return root;
}

QString ClassNetlist::equation(net_t net)
{
    Logic *lr = getLogicTree(net);
    QString equation = lr->name % " = " % Logic::flatten(lr);
    Logic::purge(lr);
    qDebug() << equation;
    return equation;
}

/*
 * Optimizes, in place, logic tree by coalescing suitable nodes
 */
void ClassNetlist::optimizeLogicTree(Logic **plr)
{
    Logic *lr = *plr;

    // Collapse inverters
    if (lr->op == LogicOp::Inverter)
    {
        Q_ASSERT(lr->children.count() == 1);
        // Detect two inverters back to back: cancel them out by bypassing them
        Logic *next = lr->children[0];
        if (next->op == LogicOp::Inverter)
        {
            Q_ASSERT(next->children.count() == 1);
            *plr = next->children[0];
            delete lr;
            delete next;

            return optimizeLogicTree(plr);
        }

        // Detect inverter followed by a gate and rewrite the gate operation, bypassing the inverter
        LogicOp nextOp;
        switch (next->op)
        {
            case LogicOp::And:  nextOp = LogicOp::Nand; break;
            case LogicOp::Nand: nextOp = LogicOp::And; break;
            case LogicOp::Or:   nextOp = LogicOp::Nor; break;
            case LogicOp::Nor:  nextOp = LogicOp::Or; break;
            default: nextOp = LogicOp::Nop; break;
        }
        if (nextOp != LogicOp::Nop)
        {
            next->op = nextOp;
            delete lr;
            *plr = next;
            lr = next;
        }
    }
    for (int i = 0; i < lr->children.count(); i++)
        optimizeLogicTree(&lr->children[i]);
}

void ClassNetlist::parse(Logic *root)
{
    if (root->leaf)
        return;

    net_t net0id = root->net;
    Net &net0 = m_netlist[net0id];

    // Recognize some common patterns:
    //-------------------------------------------------------------------------------
    // Detect clock gating
    //-------------------------------------------------------------------------------
    if ((net0.c1c2s.count() == 1) && (net0.c1c2s[0]->gate == nclk))
    {
        visitedTran.append(net0.c1c2s[0]->id);
        net_t netid = (net0.c1c2s[0]->c1 == net0id) ? net0.c1c2s[0]->c2 : net0.c1c2s[0]->c1; // Pick the "other" net
        qDebug() << net0id << "Clock gate to" << netid;
        Logic *node = new Logic(netid);
        root->children.append(new Logic(nclk)); // Add clk subnet
        root->children.append(node);
        root->op = LogicOp::And;
        parse(node);
        return;
    }

    //-------------------------------------------------------------------------------
    // Detect a lone Inverter driver
    //-------------------------------------------------------------------------------
    if ((net0.c1c2s.count() == 1) && (net0.c1c2s[0]->c2 == ngnd))
    {
        net_t netgt = net0.c1c2s[0]->gate;
        qDebug() << net0id << "Inverter to" << netgt;
        Logic *node = new Logic(netgt);
        root->children.append(node);
        root->op = LogicOp::Inverter;
        parse(node);
        return;
    }

    //-------------------------------------------------------------------------------
    // Detect Pulled-up Inverter driver (ex. 1304)
    //-------------------------------------------------------------------------------
    if ((net0.gates.count() == 0) && (net0.c1c2s.count() == 2) && net0.hasPullup)
    {
        // No gates but two source/drain connections: we need to find out which transistor we need to follow
        net_t netid = 0;
        bool x11 = visitedNets.contains(net0.c1c2s[0]->c1);
        bool x12 = visitedNets.contains(net0.c1c2s[0]->c2);
        bool x21 = visitedNets.contains(net0.c1c2s[1]->c1);
        bool x22 = visitedNets.contains(net0.c1c2s[1]->c2);
        bool gnd1 = net0.c1c2s[0]->c2 == ngnd;
        bool gnd2 = net0.c1c2s[1]->c2 == ngnd;
        // Identify which way we go
        if (gnd1 && x21 && x22)
            netid = net0.c1c2s[0]->gate;
        if (gnd2 && x11 && x12)
            netid = net0.c1c2s[1]->gate;
        if (netid)
        {
            qDebug() << net0id << "Pulled-up Inverter to" << netid;
            Logic *node = new Logic(netid);
            root->children.append(node);
            root->op = LogicOp::Inverter;
            parse(node);
            return;
        }
    }

    //-------------------------------------------------------------------------------
    // Detect Push-Pull driver
    //-------------------------------------------------------------------------------
    if (net0.c1c2s.count() == 2)
    {
        if ((net0.c1c2s[0]->c2 <= npwr) && (net0.c1c2s[1]->c2 <= npwr))
        {
            net_t netid = 0;
            net_t net01id = net0.c1c2s[0]->gate;
            net_t net02id = net0.c1c2s[1]->gate;
            Net &net01 = m_netlist[net01id];
            Net &net02 = m_netlist[net02id];
            // Identify the "short" path
            if ((net01.gates.count() == 1) && (net01.c1c2s.count() == 1) && (net01.c1c2s[0]->c2 == ngnd) && (net01.c1c2s[0]->gate == net02id))
                netid = net02id;
            if ((net02.gates.count() == 1) && (net02.c1c2s.count() == 1) && (net02.c1c2s[0]->c2 == ngnd) && (net02.c1c2s[0]->gate == net01id))
                netid = net01id;
            if (netid)
            {
                qDebug() << "Push-Pull driver" << net01id << net02id << "source net" << netid;
                Logic *node = new Logic(netid);
                root->children.append(node);
                root->op = LogicOp::Inverter;
                parse(node);
                return;
            }
        }
    }

    //-------------------------------------------------------------------------------
    // *All* source/drain connections could simply form a NOR gate with 2 or more inputs
    // This is a shortcut to a more generic case handled below (with mixed NOR/NAND/AND gates)
    //-------------------------------------------------------------------------------
    auto i = std::count_if(net0.c1c2s.begin(), net0.c1c2s.end(), [=](Trans *t) { return t->c2 == ngnd; } );
    if (i == net0.c1c2s.count())
    {
        qDebug() << "NOR gate" << net0id << "with fan-in of" << i;
        for (auto k : net0.c1c2s)
        {
            Logic *node = new Logic(k->gate);
            root->children.append(node);
        }
        root->op = LogicOp::Nor;
        return;
    }

    // Loop over all remaining transistors for which the current net is the source/drain
    for (auto t1 : net0.c1c2s)
    {
        net_t net1gt = t1->gate;
        net_t net1id = (t1->c1 == net0id) ? t1->c2 : t1->c1; // Pick the "other" net
        Net &net1 = m_netlist[net1id];

        qDebug() << "Processing net" << net0id << "at transistor" << t1->id;
        //-------------------------------------------------------------------------------
        // Ignore transistors already processed (like in a clk gating)
        //-------------------------------------------------------------------------------
        if (visitedTran.contains(t1->id))
            continue;
        //-------------------------------------------------------------------------------
        // Ignore pull-up inputs
        //-------------------------------------------------------------------------------
        if (t1->c2 == npwr)
        {
            qDebug() << "Connection to npwr ignored";
            continue;
        }

        Logic *node = new Logic(net1gt);
        root->children.append(node);

        //-------------------------------------------------------------------------------
        // Single input to a (root) NOR gate
        //-------------------------------------------------------------------------------
        // This could be an inverter but it's not since we already detected a lone input inverter driver
        // So, this is one NOR gate input in a mixed net configuration
        if (t1->c2 == ngnd)
        {
            qDebug() << "NOR gate input" << net1gt;
            parse(node);
            continue;
        }

        //-------------------------------------------------------------------------------
        // Complex NAND gate extends for 2 pass-transistor nets (ex. net 215)
        //-------------------------------------------------------------------------------
        if ((net1.gates.count() == 0) && (net1.c1c2s.count() == 2) && (net1.hasPullup == false))
        {
            Trans *t2 = (net1.c1c2s[0]->id == t1->id) ? net1.c1c2s[1] : net1.c1c2s[0];
            net_t net2gt = t2->gate;
            net_t net2id = (t2->c1 == net1id) ? t2->c2 : t2->c1; // Pick the "other" net
            Net &net2 = m_netlist[net2id];

            if (net2id == ngnd)
            {
                qDebug() << "NAND gate with 2 inputs" << net1gt << net2gt;

                node->rename(net1id);
                node->op = LogicOp::Nand;
                node->children.append(new Logic(net1gt));
                node->children.append(new Logic(net2gt));

                // Recurse into each of the contributing nets
                parse(node->children[0]);
                parse(node->children[1]);
                continue;
            }
            //-------------------------------------------------------------------------------
            // Complex NAND gate extends for 3 pass-transistor nets (ex. net 235)
            //-------------------------------------------------------------------------------
            if ((net2.gates.count() == 0) && (net2.c1c2s.count() == 2) && (net2.hasPullup == false))
            {
                Trans *t3 = (net2.c1c2s[0]->id == t2->id) ? net2.c1c2s[1] : net2.c1c2s[0];
                net_t net3gt = t3->gate;
                net_t net3id = (t3->c1 == net2id) ? t3->c2 : t3->c1; // Pick the "other" net

                if (net3id == ngnd)
                {
                    qDebug() << "NAND gate with 3 inputs" << net1gt << net2gt << net3gt;

                    node->rename(net2id);
                    node->op = LogicOp::Nand;
                    node->children.append(new Logic(net1gt));
                    node->children.append(new Logic(net2gt));
                    node->children.append(new Logic(net3gt));

                    // Recurse into each of the contributing nets
                    parse(node->children[0]);
                    parse(node->children[1]);
                    parse(node->children[2]);
                    continue;
                }
                // Have not seen a 4 pass-transistor NAND gate in Z80, yet
                Q_ASSERT(0);
            }
            // Any other topological features that start as 2 pass-transistor nets?
            Q_ASSERT(0);
        }

        //-------------------------------------------------------------------------------
        // Transistor is a simple AND gate
        //-------------------------------------------------------------------------------
        qDebug() << "AND gate with 2 input nets" << net1gt << net1id;

        node->op = LogicOp::And;
        node->children.append(new Logic(net1gt));
        node->children.append(new Logic(net1id));

        // Recurse into each of the contributing nets
        parse(node->children[0]);
        parse(node->children[1]);
    }
    // Join parallel transistors into one (root) NOR gate
    if (root->children.count() > 1)
        root->op = LogicOp::Nor;
}

Logic::Logic(net_t n, LogicOp op) : net(n), op(op)
{
    // Terminating nets: ngnd,npwr,clk, t1..t6,m1..m6, int_reset, ... any pla wire (below)
    const static QVector<net_t> term {0,1,2,3, 115,137,144,166,134,168,155,173,163,159,209,210, 95};
    name = ::controller.getNetlist().get(n);
    if (name.isEmpty())
        name = QString::number(n);
    // We stop processing nodes at a leaf node which is either one of the predefined nodes or a detected loop
    leaf = term.contains(n) ||
            name.startsWith("pla") ||
            name.startsWith("ab") ||
            name.startsWith("db") ||
            name.startsWith("ubus") ||
            name.startsWith("vbus");
    if (visitedNets.contains(n))
        leaf = true;
    else
        visitedNets.append(n);
}

void Logic::rename(net_t n)
{
    net = n;
    name = ::controller.getNetlist().get(n);
    if (name.isEmpty())
        name = QString::number(n);
}
