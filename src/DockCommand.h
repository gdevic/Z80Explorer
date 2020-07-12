#ifndef DOCKCOMMAND_H
#define DOCKCOMMAND_H

#include <QDockWidget>

class QPlainTextEdit;
class QLineEdit;

namespace Ui { class DockCommand; }

/*
 * DockCommand implements a docking window widget that contains a line edit field where
 * user can issue internal commands and a text panel to read the output of these commands.
 */
class DockCommand : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockCommand(QWidget *parent);
    ~DockCommand();

private slots:
    void returnPressed();

private:
    bool eventFilter(QObject *, QEvent *);

private:
    Ui::DockCommand *ui;

    QPlainTextEdit *m_text;             // Command output pane
    QLineEdit *m_cmd;                   // Command input line edit widget
    QStringList m_history;              // Command history list
    int m_index {};                     // Current index into history when selecting it with up/down keys
};

#endif // DOCKCOMMAND_H
