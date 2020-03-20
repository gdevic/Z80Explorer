#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class ClassChip;
class CommandWindow;
class FormImageView;
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

public slots:
    void loadChipResources();

private slots:
    void onOpenChipDir();               // Open directory with chip resources
    void onReload();                    // Reload all chip data and reset the images
    void onNewImageView();              // Open a new view to the chip data
    void onExit();                      // Exit the application

private:
    Ui::MainWindow *ui;
    QMenu *m_menuView;                  // Pointer to the "View" menu pull-down
    QMenu *m_menuWindow;                // Pointer to the "Window" menu pull-down
    LogWindow *m_logWindow;             // Log window class and form
    CommandWindow *m_cmdWindow;         // Command window class and form
    ClassChip *m_chip;                  // Holds chip information
};

#endif // MAINWINDOW_H
