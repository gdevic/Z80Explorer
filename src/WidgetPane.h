#ifndef WIDGETPANE_H
#define WIDGETPANE_H

#include <QWidget>

class WidgetPane : public QWidget
{
    Q_OBJECT
public:
    explicit WidgetPane(QWidget *parent = nullptr);

    void paintEvent(QPaintEvent *);
};

#endif // WIDGETPANE_H
