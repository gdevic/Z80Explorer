#ifndef FORMGRAPHWINDOW_H
#define FORMGRAPHWINDOW_H

#include <QDataStream>
#include <QDockWidget>
#include <QXmlStreamWriter>
#include <QtXml/QDomDocument>

class ClassBaseGraph;

namespace Ui {
    class FormGraphWindow;
}

/**
 * This docking window is a container in which we stack various graph and value widgets,
 * optionally with a timeline or a rangeslider widget at the bottom.
 */
class FormGraphWindow : public QDockWidget
{
    Q_OBJECT

public:
    explicit FormGraphWindow(QWidget *parent, QString name);
    ~FormGraphWindow();

    void saveState(QXmlStreamWriter &);                 // Save its state as an xml fragment
    void loadState(QDomNode &);                         // Load its state from an Dom node fragment

    // Creates a new view widget of the viewType and adds a list of data sources to it
    void createView(const QString viewType, QStringList ds, QString tag=QString(), bool allowEmpty=false);
    QStringList const listDs() const;                   // Return a list of all data source names that are part of this container
    void removeDs(QString);                             // Remove a particular data source from all views in this container
    void setDataCursorAt(int);                          // Send a set cursor message to all views in this dock
    void stateChanged(bool running);                    // Called when idle/running state changes

signals:
    void removeGraph(QString);                          // Signal to the owner to remove this graph window
    void onMove(QMoveEvent *event);                     // Signal to the dock window manager that a window is moved

signals:
    void dataSourceAdded(QString dsName);               // Widget signals that a data source has been added
    void dataSourceRemoved(QString dsName);             // Widget signals that a data source has been removed
    void dataSourcesChanged();                          // Widget signals that a set of data sources has been changed
    void cursorChanged(int);                            // Widget signals that a data cursor is being set

protected:
    void closeEvent(QCloseEvent *event);                // Called when the window is about to be closed
    void moveEvent(QMoveEvent *event);                  // Called when the window is moved

private:
    Ui::FormGraphWindow *ui;
    QString m_name;                                     // Name of this widget
    QList<ClassBaseGraph *> m_views;                    // List of various view widgets stacked in this window
};

#endif // FORMGRAPHWINDOW_H
