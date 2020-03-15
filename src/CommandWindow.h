#ifndef COMMANDWINDOW_H
#define COMMANDWINDOW_H

#include <QDockWidget>
#include <QPlainTextEdit>

namespace Ui {
class CommandWindow;
}

/**
 * CommandWindow implements a docking window widget that contains text field where
 * user can issue internal commands and read the output from these commands.
 */
class CommandWindow : public QDockWidget
{
    Q_OBJECT

public:
    explicit CommandWindow(QWidget *parent);                // Class constructor
    ~CommandWindow();                                       // Class destructor

signals:
    void run(QString);                                  // Signal issuing a command to run

public slots:
    void appendText(QString);                           // Called to append message into the window

private:
    bool eventFilter(QObject *, QEvent *);              // Event filter for the text widget

private:
    Ui::CommandWindow *ui;
    QPlainTextEdit *m_edit;                             // Shortcut to the text edit widget
};

#endif // COMMANDWINDOW_H
