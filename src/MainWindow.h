#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class ClassChip;
class ClassSim;
class ClassSimX;
class CommandWindow;
class LogWindow;

namespace Ui { class MainWindow; }

/*
 * Main window class
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void loadResources();               // Loads application resources
    void onOpenChipDir();               // Open directory with chip resources
    void onNewImageView();              // Open a new view to the chip data
    void onNewWaveformView();           // Open a new view to the waveforms
    void onExit();                      // Exit the application

private:
    Ui::MainWindow *ui;
    ClassChip *m_chip;                  // Holds chip information
    ClassSim *m_sim;                    // Interface to netlist simulation code
    ClassSimX *m_simx;                  // Chip netlist simulator code
    LogWindow *m_logWindow;             // Log window class and form
    CommandWindow *m_cmdWindow;         // Command window class and form
    QMenu *m_menuView;                  // Pointer to the "View" menu pull-down
    QMenu *m_menuWindow;                // Pointer to the "Window" menu pull-down
};

#endif // MAINWINDOW_H
