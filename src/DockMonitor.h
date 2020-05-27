#ifndef DOCKMONITOR_H
#define DOCKMONITOR_H

#include <QDockWidget>

namespace Ui { class DockMonitor; }

/*
 * This dock window shows information about the running simulation
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
    void onRunStopped(uint);            // Called by the sim when the current run stops at a given half-cycle

private:
    Ui::DockMonitor *ui;
    QString m_lastLoadedHex;            // Path and file name of the last loaded user program
};

#endif // DOCKMONITOR_H
