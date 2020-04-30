#ifndef WIDGETIMAGEOVERLAY_H
#define WIDGETIMAGEOVERLAY_H

#include <QWidget>

namespace Ui { class WidgetImageOverlay; }

/*
 * This class implements the overlay widget shown in the WidgetImageView
 */
class WidgetImageOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetImageOverlay(QWidget *parent);
    ~WidgetImageOverlay();

    void setImageNames(QStringList images);
    void setInfoLine(QString text);

signals:
    void actionCoords();
    void actionFind(QString text);      // New text entered in the "Find" edit box
    void actionSetImage(int i);         // Set image by index

public slots:
    void onPointerData(int x, int y);
    void selectImage(QString name, bool compose);

private slots:
    void on_btCoords_clicked();
    void onFind();                      // Called by the editFind edit widget when the user presses the Enter key

private:
    Ui::WidgetImageOverlay *ui;
};

#endif // WIDGETIMAGEOVERLAY_H
