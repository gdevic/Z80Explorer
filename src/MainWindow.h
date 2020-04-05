#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class DockCommand;
class DockLog;
class DockMonitor;

namespace Ui { class MainWindow; }

/*
 * Main window class
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent, DockLog *logWindow);
    ~MainWindow();

private slots:
    void onNewImageView();              // Open a new view to the chip data
    void onNewWaveformView();           // Open a new view to the waveforms
    void onEditWatchlist();             // Edits watchlist
    void onLoadWatchlist();             // Loads watchlist
    void onSaveWatchlistAs();           // Saves watchlist as
    void onSaveWatchlist();             // Saves current watchlist
    void onExit();                      // Exit the application
    void closeEvent(QCloseEvent *event);// Called on application close event

private:
    Ui::MainWindow *ui;
    DockLog *m_logWindow;               // Log window class and form
    DockCommand *m_cmdWindow;           // Command window class and form
    DockMonitor *m_monitor;             // Z80 environment monitor window
    QMenu *m_menuView;                  // Pointer to the "View" menu pull-down
    QMenu *m_menuWindow;                // Pointer to the "Window" menu pull-down
};

#endif // MAINWINDOW_H
