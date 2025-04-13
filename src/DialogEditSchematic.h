#ifndef DIALOGEDITSCHEMATIC_H
#define DIALOGEDITSCHEMATIC_H

#include <QDialog>

namespace Ui { class DialogEditSchematic; }

/*
 * This class contains code and UI to edit schematics properties
 */
class DialogEditSchematic : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditSchematic(QWidget *parent = nullptr);
    ~DialogEditSchematic();
    static void init();

private slots:
    void accept() override;

private:
    Ui::DialogEditSchematic *ui;
};

#endif // DIALOGEDITSCHEMATIC_H
