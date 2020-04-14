#ifndef WIDGETTOOLBAR_H
#define WIDGETTOOLBAR_H

#include <QWidget>

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

private:
    Ui::WidgetToolbar *ui;
};

#endif // WIDGETTOOLBAR_H
