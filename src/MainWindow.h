#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class ClassChip;
class ClassDockCollection;
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
    void onNewImageView();
    void onExit();                      // Exit the application

private:
    Ui::MainWindow *ui;
    QMenu *m_menuView;                  // Pointer to the "View" menu pull-down
    QMenu *m_menuWindow;                // Pointer to the "Window" menu pull-down
    LogWindow *m_logWindow;             // Log window class and form
    CommandWindow *m_cmdWindow;         // Command window class and form
    ClassDockCollection *m_docks;       // Manages user graph windows
    ClassChip *m_chip;                  // Holds chip information
    FormImageView *m_iview;             // Central image view
};

#endif // MAINWINDOW_H
