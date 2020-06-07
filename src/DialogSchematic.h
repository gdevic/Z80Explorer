#ifndef DIALOGSCHEMATIC_H
#define DIALOGSCHEMATIC_H

#include <QDialog>

namespace Ui { class DialogSchematic; }

/*
 * This dialog contains the code and UI to show the schematic layout of a selected net
 */
class DialogSchematic : public QDialog
{
    Q_OBJECT

public:
    explicit DialogSchematic(QWidget *parent = nullptr);
    ~DialogSchematic();

private:
    Ui::DialogSchematic *ui;
};

#endif // DIALOGSCHEMATIC_H
