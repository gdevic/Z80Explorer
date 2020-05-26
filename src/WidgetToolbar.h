#ifndef WIDGETTOOLBAR_H
#define WIDGETTOOLBAR_H

#include <QWidget>

namespace Ui { class WidgetToolbar; }

/*
 * This widget provides a convenient UI toolbox to run the simulation
 */
class WidgetToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetToolbar(QWidget *parent = nullptr);
    ~WidgetToolbar();

private slots:
    void doRestart();
    void onRunStopped(uint hcycle);
    void onHeartbeat(uint hcycle);

private:
    Ui::WidgetToolbar *ui;
    uint m_blinkPhase;
};

#endif // WIDGETTOOLBAR_H
