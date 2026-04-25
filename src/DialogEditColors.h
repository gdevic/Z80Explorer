#ifndef DIALOGEDITCOLORS_H
#define DIALOGEDITCOLORS_H

#include "ClassColors.h"
#include <QDialog>

class QTableWidgetItem;

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
    void onUp();
    void onDown();
    void onEdit();
    void onRemove();
    void onSelectionChanged();
    void onDoubleClicked(int row, int);
    void onItemChanged(QTableWidgetItem *item);
    void onSave();
    void onSaveAs();
    void accept() override;

private:
    Ui::DialogEditColors *ui;
    QStringList m_methods;
    bool m_ignoreItemChanged {false};

    void showFileName();
    void onLoad(bool merge);
    void swap(int index, int delta);
    void addItem(const colordef &cdef);
};

#endif // DIALOGEDITCOLORS_H
