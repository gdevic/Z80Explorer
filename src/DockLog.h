#ifndef DOCKLOG_H
#define DOCKLOG_H

#include <QDockWidget>

namespace Ui { class DockLog; }

/*
 * DockLog contains implementation of a dockable window showing
 * application logs in a scrollable text pane.
 */
class DockLog : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockLog(QWidget *parent);
    ~DockLog();

public slots:
    void processNewMessage(QString, bool);  // Add a new line of log as a string message
    void log(const QString &);              // Another version of appending to a log
    void onMaxLines();                      // Menu handler to set the number of lines to store
    void onLogLevel();                      // Changes the log level

private slots:
    void showContextMenu(const QPoint &pt); // Open a context menu

private:
    Ui::DockLog *ui;
};

#endif // DOCKLOG_H
