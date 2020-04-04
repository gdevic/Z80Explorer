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
    void on_btLoadHex_clicked();        // Button pressed to load a hex file into memory
    void onEcho(char c);                // Signal from the controller that a new virtual character is ready
    void onRunStopped();                // Signal from the controller that the simulation run stopped

private:
    Ui::DockMonitor *ui;
};

#endif // DOCKMONITOR_H
