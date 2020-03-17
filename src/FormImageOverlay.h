#ifndef FORMIMAGEOVERLAY_H
#define FORMIMAGEOVERLAY_H

#include <QWidget>

namespace Ui { class FormImageOverlay; }

/*
 * This class implements the overlay widget shown in the FormImageView
 */
class FormImageOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit FormImageOverlay(QWidget *parent = nullptr);
    ~FormImageOverlay();

public slots:
    void onPointerData(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void onClearPointerData();

private:
    Ui::FormImageOverlay *ui;
};

#endif // FORMIMAGEOVERLAY_H
