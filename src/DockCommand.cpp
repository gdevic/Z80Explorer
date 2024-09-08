#include "ClassController.h"
#include "ClassScript.h"
#include "DockCommand.h"
#include "ui_DockCommand.h"
#include <QMessageBox>
#include <QtGui>

DockCommand::DockCommand(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockCommand)
{
    ui->setupUi(this);

    // Set up shortucuts to the widgets for simplicity
    m_text = ui->textEdit;
    m_cmd = ui->lineEdit;

    // Set the maximum number of lines (blocks) for the text widget to hold.
    // This should prevent reported faults when the buffer gets very large.
    // This value might change, or be part of some sw setting.
    m_text->setMaximumBlockCount(4000);   // Max number of lines

    // Install the event filter to capture keys for history buffer etc.
    m_cmd->installEventFilter(this);

    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &DockCommand::returnPressed);
    connect(&::controller.getScript(), &ClassScript::response, this, [=](QString str) { m_text->appendPlainText(str); });

    m_text->appendPlainText("Type help() to list available commands\n");
}

DockCommand::~DockCommand()
{
    delete ui;
}

/*
 * The event filter is installed into the textEdit object and calls this
 * function every time there is an event such ashel user pressed on keys:
 *
 * ESC - clears the entered text
 * UP/DOWN - cycles through the history
 * PGUP - dumps the command history to the application log window
 */
bool DockCommand::eventFilter(QObject *object, QEvent *event)
{
    if ((object == m_cmd) && (event->type() == QEvent::KeyPress))
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_PageUp)
            qInfo() << m_history;
        if (keyEvent->key() == Qt::Key_Escape)
            m_cmd->clear();
        if ((keyEvent->key() == Qt::Key_Up) && m_history.count())
        {
            m_cmd->setText(m_history.at(m_index));
            if (m_index > 0)
                m_index--;
        }
        if (keyEvent->key() == Qt::Key_Down)
        {
            if (m_index < (m_history.size() - 1))
            {
                m_index++;
                m_cmd->setText(m_history.at(m_index));
            }
            else
                m_cmd->clear();
        }
    }
    return QDockWidget::eventFilter(object, event);
}

/*
 * Called when the user completes a command and presses Enter
 */
void DockCommand::returnPressed()
{
    QString text = m_cmd->text().trimmed().toLatin1();
    ::controller.getScript().exec(text);
    m_cmd->clear();

    // If the command is not empty, add it to the history, if unique
    if (!text.isEmpty())
    {
        // If the history already contains this command, make sure it is appended last
        m_history.removeAll(text);
        m_history.append(text);
        if (m_history.size() > 50) // Keep the history a manageable size
            m_history.removeFirst();
        m_index = m_history.size() - 1;
    }
}

/*
 * Supporting drag-and-drop of JavaScript files
 */
void DockCommand::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.count() != 1)
            return;
        QFileInfo fi(urls.first().toLocalFile());
        if (fi.suffix().toLower() != "js")
            return;
        m_dropppedFile = fi.absoluteFilePath();
        qDebug() << m_dropppedFile;
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void DockCommand::dropEvent(QDropEvent *)
{
    m_cmd->setText(QString("load(\"%1\")").arg(m_dropppedFile));
    returnPressed();
}
