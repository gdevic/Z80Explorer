#ifndef DIALOGSCHEMATIC_H
#define DIALOGSCHEMATIC_H

#include "AppTypes.h"
#include "ClassLogic.h"
#include <QDialog>

namespace Ui { class DialogSchematic; }

/*
 * This dialog contains the code and UI to show the schematic layout of a selected net
 */
class DialogSchematic : public QDialog
{
    Q_OBJECT

public:
    explicit DialogSchematic(QWidget *parent, Logic *lr);
    ~DialogSchematic();
    net_t id() { return m_logic->net; } // Returns the net number that this view shows

private:
    void reject() override { hide(); }  // Do not delete this dialog on [X] close, just hide it

private:
    Ui::DialogSchematic *ui;

    Logic *m_logic;                     // Logic tree
};

#endif // DIALOGSCHEMATIC_H
