#ifndef DOCKMONITOR_H
#define DOCKMONITOR_H

#include <QDockWidget>
#include <QTimer>

namespace Ui { class DockMonitor; }

/*
 * This dock window shows information about the running simulation
 * It also provides controls to re/load test code and is a drop target for hex files to be loaded
 */
class DockMonitor : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockMonitor(QWidget *parent = nullptr);
    ~DockMonitor();

private slots:
    void onLoad();                      // Loads user program
    void onReload();                    // Reloads last loaded user program
    void onEcho(char);                  // Write out a character to the virtual console
    void onEcho(QString);               // Write out a string to the virtual console
    void refresh();                     // Refresh the monitor information text

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *) override;

private:
    Ui::DockMonitor *ui;
    QString m_dropppedFile;             // File name of the file being dropped by a drag-and-drop operation
    QTimer  m_timer;                    // Text refresh timer updates text every 1/2 seconds to show new data
};

#endif // DOCKMONITOR_H
