#ifndef DIALOGSCHEMATIC_H
#define DIALOGSCHEMATIC_H

#include "AppTypes.h"
#include "ClassLogic.h"
#include "ClassNetlist.h"
#include <QDialog>
#include <QGraphicsScene>
#include <QMenu>

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
    // Since this is implemented as a dialog, release resources on close (don't wait on the app's end destructor)
    void closeEvent(QCloseEvent *) override
    {
        Logic::purge(m_logic);
        m_logic = nullptr;
    }

signals:
    void doShow(QString net);           // Signal back to the image view widget to show the named net
    void doNewSchematic(net_t net);     // Signal back to the image view widget to create a new schematic view

private slots:
    void onShow();                      // Menu handler to show selected net
    void onNewSchematic();              // Menu handler to create a new schematic view
    void onPng();                       // Menu handler to export diagram view as a PNG image file
    void onSettings();                  // Menu handler to show the schematic settings dialog

private:
    void createDrawing();               // Creates drawing outside of the constructor
    void drawSymbol(QPoint loc, Logic *lr); // Recursively draws symbols and connecting lines
    int preBuild(Logic *lr);            // Pre-builds the tree to calculate screen positions
    QAction *m_actionShow = new QAction("Show", this);
    QAction *m_actionNew = new QAction("Schematic...", this);
    QAction *m_actionPng = new QAction("Export PNG...", this);
    QAction *m_actionSettings = new QAction("Settings...", this);

private:
    Ui::DialogSchematic *ui;

    Logic *m_logic;                     // Logic tree
    QGraphicsScene *m_scene;            // Graphics scene we are painting to
    QMenu *m_menu = new QMenu(this);    // Context menu
};

#endif // DIALOGSCHEMATIC_H
