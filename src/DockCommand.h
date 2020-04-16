#ifndef DOCKCOMMAND_H
#define DOCKCOMMAND_H

#include <QDockWidget>

class QPlainTextEdit;

namespace Ui { class DockCommand; }

/*
 * DockCommand implements a docking window widget that contains text field where
 * user can issue internal commands and read the output from these commands.
 */
class DockCommand : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockCommand(QWidget *parent);
    ~DockCommand();

signals:
    void run(QString);

private slots:
    void appendText(QString);

private:
    bool eventFilter(QObject *, QEvent *);

    Ui::DockCommand *ui;
    QPlainTextEdit *m_edit;
};

#endif // DOCKCOMMAND_H
