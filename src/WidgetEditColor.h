#ifndef WIDGETEDITCOLOR_H
#define WIDGETEDITCOLOR_H

#include <QDialog>
#include "ClassColors.h"

namespace Ui { class WidgetEditColor; }

/*
 * This widget (a dialog) contains the code and UI to edit a single custom color matching definition
 */
class WidgetEditColor : public QDialog
{
    Q_OBJECT

public:
    explicit WidgetEditColor(QWidget *parent, QStringList methods);
    ~WidgetEditColor();

    void set(const colordef &cdef);     // Sets a single custom color matching definition to be edited
    void get(colordef &cdef);           // Returns edited custom color matching definition

private slots:
    void onColor();
    void onTextChanged(const QString &text);

private:
    Ui::WidgetEditColor *ui;
    QColor m_color;

    QString style(const QColor color)   // Returns a style sheet for our color-ized button
    { return QString("background-color: rgba(%1, %2, %3, 1.0); border: 1px solid black; border-radius: 3px;")
                .arg(color.red()).arg(color.green()).arg(color.blue()); }
};

#endif // WIDGETEDITCOLOR_H
