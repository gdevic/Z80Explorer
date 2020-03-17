#include "FormGraphWindow.h"
#include "ui_FormGraphWindow.h"

#include <QtGui>

/*
 * This widget is a container in which we stack graph widgets
 * with the timeline at the bottom. It is then added to the main window.
 */
FormGraphWindow::FormGraphWindow(QWidget *parent, QString name) :
    QDockWidget(parent),
    ui(new Ui::FormGraphWindow),
    m_name(name)
{
    ui->setupUi(this);
}

/*
 * Standard UI class destructor
 */
FormGraphWindow::~FormGraphWindow()
{
    delete ui;
}

/*
 * Event triggered when a graph window is about to be closed.
 * Emit a signal to broadcast that message to dock window collection that is keeping
 * track of docking graph windows.
 */
void FormGraphWindow::closeEvent(QCloseEvent *event)
{
    emit removeGraph(objectName());
}

/*
 * A dock window is being moved. If the user pressed a Ctrl key, pass the move event to
 * the dock window manager so it can move all dock windows!
 */
void FormGraphWindow::moveEvent(QMoveEvent *event)
{
    QDockWidget::moveEvent(event);
    if (QApplication::keyboardModifiers()==Qt::ControlModifier)
        emit onMove(event);
}

/*
 * Creates a new view widget and adds a number of data sources to it.
 * The method will remove any duplicate and non-existing data sources. If "allowEmpty" is true,
 * it will create a view even if the final list of data sources is empty. That is used on loading.
 */
void FormGraphWindow::createView(const QString viewType, QStringList dsList, QString tag, bool allowEmpty)
{
    // Widgets will be added into the scroll area of its vertical layout
    QLayout *layout = ui->layoutScroll;
}

/*
 * Return a list of all data source names that are part of this graphing window
 */
QStringList const FormGraphWindow::listDs() const
{
    QStringList dsList;
    return dsList;
}

/*
 * Remove a particular data source from all views in this container
 */
void FormGraphWindow::removeDs(QString name)
{
}

/*
 * Send a set cursor message to all views in this dock
 */
void FormGraphWindow::setDataCursorAt(int index)
{
}

/*
 * Called when idle/running state changes
 */
void FormGraphWindow::stateChanged(bool running)
{
}

/*
 * Saves internal state to xml writer as a fragment. This function is to be used
 * only as part of a larger save state
 */
void FormGraphWindow::saveState(QXmlStreamWriter &xmlWriter)
{
}

/*
 * Loads internal state from a Dom node. This function is to be used
 * only as part of a larger load state
 */
void FormGraphWindow::loadState(QDomNode &node)
{
}
