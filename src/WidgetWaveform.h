#ifndef WIDGETWAVEFORM_H
#define WIDGETWAVEFORM_H

#include <QWidget>

class WidgetWaveform : public QWidget
{
    Q_OBJECT
public:
    explicit WidgetWaveform(QWidget *parent = nullptr);

    void paintEvent(QPaintEvent *);
};

#endif // WIDGETWAVEFORM_H
