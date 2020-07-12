#ifndef DIALOGEDITWAVEFORM_H
#define DIALOGEDITWAVEFORM_H

#include "DockWaveform.h"
#include <QDebug>
#include <QDialog>
#include <QListWidgetItem>

namespace Ui { class DialogEditWaveform; }

/*
 * This class contains the code and UI to manage waveform view data
 */
class DialogEditWaveform : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditWaveform(QWidget *parent, QVector<viewitem> list);
    ~DialogEditWaveform();

    void getList(QVector<viewitem> &list); // Returns (by setting "list") the edited list of view items

private slots:
    void onAdd();
    void onRemove();
    void onUp();
    void onDown();
    void onColor();
    void allSelChanged();
    void viewSelChanged();
    void onFormatIndexChanged(int);

private:
    // current working viewitem data is kept in each list widget item as a QVariant field data()
    viewitem get(QListWidgetItem *item);
    void set(QListWidgetItem *item, viewitem &view);
    void append(viewitem &view);

private:
    Ui::DialogEditWaveform *ui;
};

#endif // DIALOGEDITWAVEFORM_H
