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
    explicit DialogSchematic(QWidget *parent, net_t net, Logic *lr);
    ~DialogSchematic();
    net_t id() { return m_net; }        // Returns the net number that this view shows

private:
    void reject() override { hide(); }  // Do not delete this dialog on [X] close, just hide it

private:
    Ui::DialogSchematic *ui;

    net_t m_net;                        // Net number that this schematic view shows
    Logic *m_logic;                     // Logic tree
};

#endif // DIALOGSCHEMATIC_H
