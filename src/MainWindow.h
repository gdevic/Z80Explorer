#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>

class ClassChip;
class ClassDockCollection;
class CommandWindow;
class LogWindow;
class FormImageView;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool init();                                        // Initialization outside the constructor

public slots:
    void onOpenChipDir(); // Open directory with chip resources
    void onExit(); // Exit the application

private:
    Ui::MainWindow *ui;
    QMenu *m_menuView;                                  // Pointer to the "View" menu pull-down
    QMenu *m_menuWindow;                                // Pointer to the "Window" menu pull-down
    LogWindow *m_logWindow;                             // Log window class and form
    CommandWindow *m_cmdWindow;                         // Command window class and form
    ClassDockCollection *m_docks;                       // Manages user graph windows
    ClassChip *m_chip;                                  // Holds chip information
    FormImageView *m_iview;                             // Central image view
};
#endif // MAINWINDOW_H
