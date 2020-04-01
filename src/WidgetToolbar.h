#ifndef WIDGETTOOLBAR_H
#define WIDGETTOOLBAR_H

#include <QWidget>

namespace Ui {
class WidgetToolbar;
}

class WidgetToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetToolbar(QWidget *parent = nullptr);
    ~WidgetToolbar();

private:
    Ui::WidgetToolbar *ui;
};

#endif // WIDGETTOOLBAR_H
