#ifndef CLASSDOCKWINDOWCOLLECTION_H
#define CLASSDOCKWINDOWCOLLECTION_H

#include <QObject>
#include <QStringList>

class FormGraphWindow;
class MainWindow;
class QDockWidget;
class QMoveEvent;
class QRect;

/**
 * This class contains code to manage a collection of user dockable windows,
 * in particular a set of graphing windows which need to be saved and restored
 * across workspace sessions.
 *
 * The other major function of this class is to serve as a model for the user graph
 * widget view. That is a tree view showing all user dock windows and their data sources.
 */
class ClassDockCollection: public QObject
{
    Q_OBJECT

public:
    ClassDockCollection(MainWindow *);
    ~ClassDockCollection();
    QList<FormGraphWindow *> load(QString fileName);                // Load and recreate user graph windows
    void save(QString fileName) const;                              // Save all user graph windows to a file
    QStringList getTitles() const;                                  // Return a list of docking window titles

public slots:
    void renameTitle(QString oldTitle, QString newTitle);           // Rename the title of a docking window (whose existing title is oldTitle)
    void createDock(QStringList ds);                                // Create a new dock window containing a set of graph views
    void closeDock(QString name);                                   // Close a docking window specified by its name
    void closeAllDocks();                                           // Close all graph docking windows
    void tile(QRect &avoid);                                        // Tile all floating windows while avoiding a rectangle

public slots:
    void dataSourceAdded(QString dsName);                           // Widget signals that a data source has been added
    void dataSourceRemoved(QString dsName);                         // Widget signals that a data source has been removed
    void dataSourcesChanged();                                      // Widget signals that a set of data sources has been changed
    void cursorChanged(int);                                        // Widget signals that a data cursor is being set
    void stateChanged(bool running);                                // Called when the collection idle/running state changes
    void onMove(QMoveEvent *event);                                 // Dock window is being moved

signals:
    void rebuildView();                                             // Signals to the view models to rebuild the view
    void setDataCursorAt(int);                                      // Signals to the view models to set data cursor

private:                                                            // Private methods:
    QList<FormGraphWindow *> getGraphDocks() const;                 // Return a list of all graph docking windows
    FormGraphWindow *createGraphContainer(const QString title);     // Create a docking window container

private:
    friend class ViewWindow;                                        // ViewWindow is a view of this data; let it access this class.

    MainWindow *m_main;                                             // Pointer to the main window to access menus and docks
    uint m_windowId;                                                // Running ID of a dock window to help us making unique names
    QList<FormGraphWindow *> m_docks;                               // Internal cache of window docks
    bool m_inMove;                                                  // Prevent the move window recursion
};

#endif // CLASSDOCKWINDOWCOLLECTION_H
