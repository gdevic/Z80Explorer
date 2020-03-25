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
    explicit FormImageOverlay(QWidget *parent);
    ~FormImageOverlay();

    void setLayerNames(QStringList layers);

signals:
    void actionBuild();
    void actionCoords();
    void actionFind(QString text);      // New text entered in the "Find" edit box

public slots:
    void onPointerData(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void onClearPointerData();
    void setText(int index, QString text);

private slots:
    void on_btBuild_clicked();
    void on_btCoords_clicked();
    void onFind();                      // Called by the editFind edit widget when the user presses the Enter key

private:
    Ui::FormImageOverlay *ui;
};

#endif // FORMIMAGEOVERLAY_H
