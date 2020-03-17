#ifndef CLASSBASEGRAPH_H
#define CLASSBASEGRAPH_H

#include <QStringList>

/*
 * This class defines interface to various graphing (view) widget classes.
 */
class ClassBaseGraph
{
public:
    // Return a name of one of the view types to which this graph belongs
    virtual const QString getViewType() = 0;

    // Return the custom view data to be saved when serializing
    virtual const QString getSaveData() { return QString(); }

    // Dynamically add a single data source to the view
    virtual void addDs(QString) = 0;

    // Dynamically add a list of data sources to the view
    virtual void addDs(QStringList);    // Has a default implementation

    // Dynamically remove a single data source from the view
    virtual void removeDs(QString) = 0;

    // Dynamically remove a list of data sources from the view
    virtual void removeDs(QStringList); // Has a default implementation

    // Return a list of all data sources that are part of this view
    virtual QStringList const listDs()  { return m_dsList; }

    // Return a custom view tag string to be serialized
    virtual const QString getTag()      { return QString(); };

    // If a view supports data cursor, this function should set it
    virtual void setDataCursorAt(int)   { return; };

protected:
    QStringList m_dsList;                       // List of all data sources within this view
};

#endif // CLASSBASEGRAPH_H
