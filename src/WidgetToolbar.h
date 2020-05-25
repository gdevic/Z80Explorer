#ifndef WIDGETTOOLBAR_H
#define WIDGETTOOLBAR_H

#include <QWidget>
#include <QTimer>

namespace Ui { class WidgetToolbar; }

/*
 * This widget provides a convenient interface to run the simulation
 */
class WidgetToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetToolbar(QWidget *parent = nullptr);
    ~WidgetToolbar();

public slots:
    void onRunStopped(uint); // Called by the sim when the current run stops at a given half-cycle

private slots:
    void doRestart();
    void onTimeout();

private:
    Ui::WidgetToolbar *ui;
    QTimer m_timer; // Timer to blink / indicate that the simulation is running
    uint m_blinkPhase;
};

#endif // WIDGETTOOLBAR_H
