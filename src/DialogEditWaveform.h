#ifndef DIALOGEDITWAVEFORM_H
#define DIALOGEDITWAVEFORM_H

#include "DockWaveform.h"
#include <QDebug>
#include <QDialog>

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

    QVector<viewitem> &get()            // Return the edited list of view items
        { return m_view; }

private slots:
    void onAdd();
    void onRemove();
    void onUp();
    void onDown();
    void allSelChanged();
    void viewSelChanged();
    void showEvent(QShowEvent *) override;

private:
    bool contains(QString name);

private:
    Ui::DialogEditWaveform *ui;

    QVector<viewitem> m_view;           // A (local) collection of view items we are editing
};

#endif // DIALOGEDITWAVEFORM_H
