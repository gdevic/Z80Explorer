#include "ClassDockCollection.h"
#include "ClassException.h"
#include "FormGraphWindow.h"
#include "MainWindow.h"
#include <QtGui>
#include <QDebug>

#include <QDesktopWidget>
#include <QApplication>
#include <QMenuBar>

/**
 * This class contains code to manage a collection of user dockable windows,
 * in particular a set of graphing windows which need to be saved and restored
 * across workspace sessions.
 */
ClassDockCollection::ClassDockCollection(MainWindow *main) : QObject(0),
    m_main(main),
    m_windowId(0),
    m_inMove(false)
{
}

/**
 * Class destructor
 */
ClassDockCollection::~ClassDockCollection()
{
    closeAllDocks();
}

/**
 * Close a docking window specified by its name.
 * This method removes a dock from the lists and actually deletes it.
 */
void ClassDockCollection::closeDock(QString name)
{
    // Loop over all graph windows and close the one whose name matches
    foreach(FormGraphWindow *dock, m_docks)
    {
        // As usual, we search by the object name...
        if (dock->objectName()==name)
        {
            // And since it is only one, we can remove it and break out of the loop
            m_main->removeDockWidget(static_cast<QDockWidget *>(dock));
            delete dock;
            //emit m_main->updateAppTitlebar(true);
            break;
        }
    }
    m_docks = getGraphDocks();
    emit rebuildView();
}

/**
 * Removes all user graphing docking windows
 */
void ClassDockCollection::closeAllDocks()
{
    // Loop over all docking windows and close them one by one
    foreach(FormGraphWindow *dock, m_docks)
        closeDock(dock->objectName());
    m_windowId = 0;
    m_docks.clear();
    emit rebuildView();
}

/**
 * Tile all floating windows in two directions while avoiding a given rectangle area
 */
void ClassDockCollection::tile(QRect &avoid)
{
    // Get the primary desktop size so we can tile graph windows
    QDesktopWidget desktop;
    int desktopHeight=desktop.geometry().height();
    int desktopWidth=desktop.geometry().width();
    int x = 0, y = 0;       // Running tile starting coordinate
    int w = 0, h = 0;       // Width and height of a reference dock window frame
    QRect inner;            // Inner frame of a widget within a dock window

    m_inMove = true;        // This function share this semaphore to prevent move recursion
    foreach(FormGraphWindow *dock, m_docks)
    {
        if (!dock->isFloating())    // Ignore docks that are not already floating
            continue;

        // Store the graph window size by reading it from the first graph dock window
        if (w==0)
        {
            QRect frame = dock->frameGeometry();
            w = frame.width(); h = frame.height();
            inner = dock->geometry();
        }
        do
        {
            dock->move(x,y);
            dock->resize(inner.width(), inner.height());
            // Advance the corner point in two different ways:
            // By default, fill in the columns and then start over on rows
            // If the user holds Ctrl key, fill in the columns first
            if (QApplication::keyboardModifiers()==Qt::ControlModifier)
            {
                y += h;
                if (y+h >= desktopHeight)
                {
                    y = 0;
                    x += w;
                    if (x>= desktopWidth)
                        break;
                }
            }
            else
            {
                x += w;
                if (x+w >= desktopWidth)
                {
                    x = 0;
                    y += h;
                    if (y >= desktopHeight)
                        break;
                }
            }
        } while(avoid.intersects(dock->frameGeometry()));
    }
    m_inMove = false;
}

/**
 * Dock window is being moved - move along all dock windows
 * The check for Ctrl key is done in the caller, so this function
 * simply moves all dock windows per event parameters
 */
void ClassDockCollection::onMove(QMoveEvent *event)
{
    if (m_inMove) return;
    m_inMove = true;
    foreach(FormGraphWindow *dock, m_docks)
    {
        if (!dock->isFloating())    // Ignore docks that are not already floating
            continue;
        QRect inner = dock->geometry();
        if (inner.x()!=event->pos().x() || inner.y()!=event->pos().y())
        {
            dock->move(dock->pos() + (event->pos() - event->oldPos()));
        }
    }
    m_inMove = false;
}

/**
 * A widget signals that a data source has been added
 */
void ClassDockCollection::dataSourceAdded(QString dsName)
{
    //emit m_main->updateAppTitlebar(true);
    emit rebuildView();
}

/**
 * A widget signals that a data source has been removed
 */
void ClassDockCollection::dataSourceRemoved(QString dsName)
{
    //emit m_main->updateAppTitlebar(true);
    emit rebuildView();
}

/**
 * A widget signals that a set of data sources has been changed
 */
void ClassDockCollection::dataSourcesChanged()
{
    //emit m_main->updateAppTitlebar(true);
    emit rebuildView();
}

/**
 * A widget signals that a data cursor is being set
 * Set data cursor to a specific database index
 * If the index is a negative number, remove or hide the data cursor
 */
void ClassDockCollection::cursorChanged(int index)
{
    foreach(FormGraphWindow *dock, m_docks)
        dock->setDataCursorAt(index);
}

/**
 * Called when the collection idle/running state changes
 */
void ClassDockCollection::stateChanged(bool running)
{
    // Loop over all docking windows and signal a state change
    foreach(FormGraphWindow *dock, m_docks)
        dock->stateChanged(running);
}

/**
 * Return a list of all docking window titles
 */
QStringList ClassDockCollection::getTitles() const
{
    QStringList titles;
    foreach(FormGraphWindow *graph, m_docks)
        titles << graph->objectName();

    return titles;
}

/**
 * Rename a specific dock window totle. This function will search for a named
 * dock window oldTitle and rename it to newTitle.
 */
void ClassDockCollection::renameTitle(QString oldTitle, QString newTitle)
{
    QList<FormGraphWindow *> graphWidgets = getGraphDocks();
    // Loop over all graph windows and find the one with the matching name (oldTitle)
    // and rename it to newTitle parameter
    foreach(FormGraphWindow *graph, graphWidgets)
    {
        // As usual, we search by the object name...
        if (graph->objectName()==oldTitle)
        {
            graph->setWindowTitle(newTitle);
            graph->setObjectName(newTitle);
            //m_main->updateAppTitlebar(true);
            break;
        }
    }
    m_docks = getGraphDocks();
    emit rebuildView();
}

/**
 * Return a list of user graphing windows by traversing all docking windows
 * of the main class and extracting only those that have a specific signature
 * which is the object name has to contain a colon :
 */
QList<FormGraphWindow *> ClassDockCollection::getGraphDocks() const
{
    QList<FormGraphWindow *> docks;
    foreach(QDockWidget *dock, m_main->findChildren<QDockWidget *>())
    {
        // As usual, we search by the object name...
        if (dock->objectName().contains(":"))
            docks.append(static_cast<FormGraphWindow *>(dock));
    }
    return docks;
}

/**
 * This internal method creates a docking window container of the class FormGraphWindow
 * and sets it default properties.
 */
FormGraphWindow *ClassDockCollection::createGraphContainer(const QString title)
{
    // This is a container window that stores a number of graphing widgets
    FormGraphWindow *graph = new FormGraphWindow(m_main, title);

    // By default, docking of graph windows is enabled to all areas of the central pane.
    // If the user wanted to disble it, clear the allowed areas which will effectively disable docking.
    QSettings settings;
    if (settings.value("DisableDocking", false).toBool()==true)
        graph->setAllowedAreas(0);

    graph->setAttribute(Qt::WA_DeleteOnClose);
    graph->setWindowTitle(title);
    graph->setObjectName(title);
    connect(graph, SIGNAL(removeGraph(QString)), this, SLOT(closeDock(QString)));
    connect(graph, SIGNAL(dataSourceAdded(QString)), this, SLOT(dataSourceAdded(QString)));
    connect(graph, SIGNAL(dataSourceRemoved(QString)), this, SLOT(dataSourceRemoved(QString)));
    connect(graph, SIGNAL(dataSourcesChanged()), this, SLOT(dataSourcesChanged()));
    connect(graph, SIGNAL(cursorChanged(int)), this, SLOT(cursorChanged(int)));
    connect(graph, SIGNAL(onMove(QMoveEvent*)), this, SLOT(onMove(QMoveEvent*)));

    return graph;
}

/**
 * Create a new docking window and populate it with one or more graph views.
 * The calling widget's "whatsThis" property should contain the name of a view to be created.
 * A drop signal source should ensure that the given list of data sources is valid and not empty.
 */
void ClassDockCollection::createDock(QStringList ds)
{
    qDebug() << "ClassDockWindowCollection::CreateDock" << ds;

    // Depending on the sender of this signal, create different views. Each sender widget
    // contains the name of the view to create in its "whatsThis" property.
    QWidget *w = (QWidget*)QObject::sender();
    Q_ASSERT(w);

    // At this moment, valid view types are:
    // * Graph          - a line graphing widget
    // * Text-Log       - a widget showing lines of text
    // * Value          - a simple name = value list of data sources and their current values
    // * Bar            - a bar graph view
    const QString viewType = w->whatsThis();

    // Make a unique window/object name
    // The default name is a name of the first data source in it
    QString title = QString("%1:%2").arg(++m_windowId).arg(ds.first());
    FormGraphWindow *graph = createGraphContainer(title);
    graph->setFloating(true);
    graph->createView(viewType, ds);

    m_main->addDockWidget(Qt::RightDockWidgetArea, graph);
    //m_main->updateAppTitlebar(true);

    // Add an action to the menu under the main "Window" menu
    m_main->menuBar()->findChild<QMenu *>("menuWindow")->addAction(graph->toggleViewAction());

    m_docks = getGraphDocks();
    emit rebuildView();
}

/**
 * Load graphing windows from the given file. This is a top-level file-load function.
 * Creates windows accordingly and returns a list of docking windows. This list should
 * be used by the caller to add dock windows to the main window class.
 */
QList<FormGraphWindow *> ClassDockCollection::load(QString fileName)
{
    m_docks.clear();

    QFile file(fileName);

    try
    {
        // Load collection from an XML formatted file using Dom
        if (!file.open(QFile::ReadOnly | QFile::Text))
            throw Exception("ClassDockWindowCollection::Load", "Error opening file " + fileName);

        QDomDocument doc;
        if (!doc.setContent(&file, false))
            throw Exception("ClassDockWindowCollection::Load setContent()");
        QDomElement root = doc.documentElement();
        if (root.tagName() != "UserGraphCollection")
            throw Exception("ClassDockWindowCollection::Load unexpected XML element");
        QDomNode child = root.firstChild();
        while(!child.isNull())
        {
            QDomElement element = child.toElement();
            if (element.tagName()=="Dock")
            {
                QString name = element.attribute("Object");
                if (name.isEmpty())
                    continue;

                // Create a docking graph window container and load it with the state as read from the XML file
                FormGraphWindow *graph = createGraphContainer(name);
                QDomNode reader = element.firstChild();
                graph->loadState(reader);
                m_docks.append(graph);
                if (!m_main->restoreDockWidget(graph))
                    m_main->addDockWidget(Qt::RightDockWidgetArea, graph);
                // Based on the ID number just loaded, update our collection counter
                // This we do to make sure future graphs have a unique object name
                uint n = m_windowId;
                if (name.contains(":"))
                    n = name.mid(0, name.indexOf(":")).toUInt();
                m_windowId = qMax(m_windowId, n);
            }
            child = child.nextSibling();
        }
        file.close();
    }
    catch(Exception &ex)
    {
        file.close();
        qCritical() << ex.origin() << ex.what();
    }
    emit rebuildView();
    return m_docks;
}

/**
 * Save graphing windows to the given file. This is a top-level file save function.
 * Save collection of graphing windows to a file with the given file name.
 */
void ClassDockCollection::save(QString fileName) const
{
    QFile file(fileName);

    try
    {
        // Save collection to a file in XML format
        if (!file.open(QIODevice::WriteOnly))
            throw Exception("ClassDockWindowCollection::Save", "Error creating file " + fileName);

        QList<FormGraphWindow *> graphWidgets = getGraphDocks();
        QXmlStreamWriter xmlWriter(&file);
        xmlWriter.setAutoFormatting(true);
        xmlWriter.writeStartDocument();
        xmlWriter.writeStartElement("UserGraphCollection");

        // Iterate over all user graph windows and write their information out
        foreach(FormGraphWindow *graph, graphWidgets)
        {
            xmlWriter.writeStartElement("Dock");
            xmlWriter.writeAttribute("Object", graph->objectName());
            graph->saveState(xmlWriter);
            xmlWriter.writeEndElement();
        }

        xmlWriter.writeEndElement();
        xmlWriter.writeEndDocument();
        if (xmlWriter.hasError())
            throw Exception("ClassDockWindowCollection::Save", "Error saving XML file");
        file.close();
    }
    catch(Exception &ex)
    {
        file.close();
        qCritical() << ex.origin() << ex.what();
    }
}
