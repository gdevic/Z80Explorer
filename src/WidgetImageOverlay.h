#ifndef WIDGETIMAGEOVERLAY_H
#define WIDGETIMAGEOVERLAY_H

#include <QWidget>

class QPushButton;

namespace Ui { class WidgetImageOverlay; }

/*
 * This class implements the overlay widget shown in the WidgetImageView
 */
class WidgetImageOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetImageOverlay(QWidget *parent, QString sid);
    ~WidgetImageOverlay();

    void setInfoLine(uint index, QString text);
    void clearInfoLine(uint index);
    void setButton(uint i, bool checked);
    void setCoords(const QString coords);
    void createImageButtons(QStringList imageNames);
    QString getLayers();                        // Returns a string containing the status of the image layer visibility

signals:
    void actionButton(int i);                   // User clicked on one of the buttons on the top
    void actionCoords();                        // User clicked on the button with coordinates
    void actionFind(QString text);              // New text entered in the "Find" edit box
    void actionSetImage(uint img, bool blend);  // Sets or blends an image by its index

public slots:
    void selectImageButton(uint img, bool blend);

private slots:
    void onFind();                              // Called by the editFind edit widget when the user presses the Enter key

private:
    Ui::WidgetImageOverlay *ui;
    QVector<QPushButton *> m_imageButtons;      // List of image buttons
};

#endif // WIDGETIMAGEOVERLAY_H
