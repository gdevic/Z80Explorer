#ifndef DOCKWAVE_H
#define DOCKWAVE_H

#include <QDockWidget>

namespace Ui { class DockWaveform; }

class DockWaveform : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockWaveform(QWidget *parent = nullptr);
    ~DockWaveform();

private:
    Ui::DockWaveform *ui;
};

#endif // DOCKWAVE_H
