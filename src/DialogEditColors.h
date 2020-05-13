#ifndef DIALOGEDITCOLORS_H
#define DIALOGEDITCOLORS_H

#include <QDialog>

namespace Ui { class DialogEditColors; }

/*
 * This class contains code to edit custom colors
 */
class DialogEditColors : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditColors(QWidget *parent = nullptr);
    ~DialogEditColors();

private:
    Ui::DialogEditColors *ui;
};

#endif // DIALOGEDITCOLORS_H
