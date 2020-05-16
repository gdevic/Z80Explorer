#ifndef DIALOGEDITCOLORS_H
#define DIALOGEDITCOLORS_H

#include <QDialog>
#include "ClassColors.h"

namespace Ui { class DialogEditColors; }

/*
 * This class contains code and UI to edit custom colors
 */
class DialogEditColors : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditColors(QWidget *parent = nullptr);
    ~DialogEditColors();

private slots:
    void onAdd();
    void onEdit();
    void onRemove();
    void onSelectionChanged();
    void onDoubleClicked(int row, int);
    void accept() override;

private:
    Ui::DialogEditColors *ui;
    QStringList m_methods;

    void addItem(const colordef &cdef);
};

#endif // DIALOGEDITCOLORS_H
