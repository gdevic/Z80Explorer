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

private slots:
    void onRun();
    void onStop();
    void onStep();

private:
    Ui::WidgetToolbar *ui;
};

#endif // WIDGETTOOLBAR_H
