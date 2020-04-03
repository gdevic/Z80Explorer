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
    void on_btLoadHex_clicked();
    void onEcho(char c);

private:
    Ui::DockMonitor *ui;
};

#endif // DOCKMONITOR_H
