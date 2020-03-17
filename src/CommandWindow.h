#ifndef COMMANDWINDOW_H
#define COMMANDWINDOW_H

#include <QDockWidget>

class QPlainTextEdit;

namespace Ui { class CommandWindow; }

/*
 * CommandWindow implements a docking window widget that contains text field where
 * user can issue internal commands and read the output from these commands.
 */
class CommandWindow : public QDockWidget
{
    Q_OBJECT

public:
    explicit CommandWindow(QWidget *parent);
    ~CommandWindow();

signals:
    void run(QString);

public slots:
    void appendText(QString);

private:
    bool eventFilter(QObject *, QEvent *);

private:
    Ui::CommandWindow *ui;
    QPlainTextEdit *m_edit;
};

#endif // COMMANDWINDOW_H
