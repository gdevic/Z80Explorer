#ifndef DIALOGSCHEMATIC_H
#define DIALOGSCHEMATIC_H

#include "AppTypes.h"
#include "ClassLogic.h"
#include <QDialog>
#include <QGraphicsScene>

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
    void createDrawing();               // Creates drawing outside of the constructor
    void drawSymbol(QPoint loc, Logic *lr); // Recursively draws symbols and connecting lines
    int preBuild(Logic *lr);            // Pre-builds the tree to calculate screen positions

private:
    Ui::DialogSchematic *ui;

    Logic *m_logic;                     // Logic tree
    QGraphicsScene *m_scene;            // Graphics scene we are painting to
};

#endif // DIALOGSCHEMATIC_H
