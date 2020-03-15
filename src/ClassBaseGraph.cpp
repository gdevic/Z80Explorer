#include "ClassBaseGraph.h"

/**
 * Dynamically add a list of data sources to the view
 */
void ClassBaseGraph::addDs(QStringList dsList)
{
    foreach(QString name, dsList)
        addDs(name);
}

/**
 * Dynamically remove a list of data sources from the view
 */
void ClassBaseGraph::removeDs(QStringList dsList)
{
    foreach(QString name, dsList)
        removeDs(name);
}
