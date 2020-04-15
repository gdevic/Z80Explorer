#include "DockCommand.h"
#include "ui_DockCommand.h"

#include <QtGui>

/*
 * DockCommand constructor.
 */
DockCommand::DockCommand(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockCommand)
{
    ui->setupUi(this);

    // Set the shortucut to the text widget for simplicity
    m_edit = ui->textEdit;

    // Set the maximum number of lines (blocks) for the text widget to hold.
    // This should prevent reported faults when the buffer gets very large.
    // This value might change, or be part of some sw setting.
    m_edit->setMaximumBlockCount(4000);   // This many lines max

    // Install the event filter to capture Return key
    // This method avoids the need to subclass QTextEdit
    m_edit->installEventFilter(this);
}

/*
 * DockCommand destructor.
 */
DockCommand::~DockCommand()
{
    delete ui;
}

/*
 * The event filter is installed into the textEdit object and calls this
 * function every time there is a event, including user pressed on a Return key
 */
bool DockCommand::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_edit && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return)
        {
            // Get the line of text that the cursor is positioned at
            QString text = m_edit->textCursor().block().text().trimmed().toLatin1() ;
            // If there was any text in the current line, issue a command
            if (text.length()>0)
                emit run(text);
        }
    }
    return QDockWidget::eventFilter(object, event);
}

/*
 * Appends text to the text window
 */
void DockCommand::appendText(QString str)
{
    m_edit->moveCursor(QTextCursor::End);
    m_edit->insertPlainText("\n" + str);
    m_edit->moveCursor(QTextCursor::End);
}
