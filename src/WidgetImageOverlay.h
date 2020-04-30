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

    void setInfoLine(QString text);
    void setCoords(int x, int y);
    void setImageNames(QStringList images);

signals:
    void actionCoords();                // User clicked on the button with coordinates
    void actionFind(QString text);      // New text entered in the "Find" edit box
    void actionSetImage(int i);         // Set image by index

public slots:
    void selectImage(QString name, bool compose);

private slots:
    void onFind();                      // Called by the editFind edit widget when the user presses the Enter key

private:
    Ui::WidgetImageOverlay *ui;
};

#endif // WIDGETIMAGEOVERLAY_H
