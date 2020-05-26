#include "DockLog.h"
#include "ui_DockLog.h"
#include "ClassApplog.h"
#include "ClassSingleton.h"

#include <QInputDialog>
#include <QMenu>
#include <QtGui>

/*
 * DockLog constructor.
 */
DockLog::DockLog(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockLog)
{
    ui->setupUi(this);

    // Set the maximum number of lines (blocks) for the text widget to hold.
    // This should prevent reported faults when the buffer gets very large.
    QSettings settings;
    int lines = settings.value("AppLogLines", 2000).toInt();
    ui->textEdit->setMaximumBlockCount(lines);

    // Alter the popup menu when user right clicks
    ui->textEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textEdit,SIGNAL(customContextMenuRequested(const QPoint&)), this,SLOT(showContextMenu(const QPoint &)));

    // Connect log message slot
    CAppLogHandler *applog = &Singleton<CAppLogHandler>::Instance();

    QObject::connect(applog, SIGNAL(NewLogMessage(QString, bool)), this, SLOT(processNewMessage(QString, bool)));
}

DockLog::~DockLog()
{
    delete ui;
}

/*
 * Slot functions that receives log messages and display them in the log window
 */
void DockLog::processNewMessage(QString message, bool newLine)
{
    Q_UNUSED(newLine)
    ui->textEdit->appendPlainText(message);
    ui->textEdit->ensureCursorVisible();
}

/*
 * Another version of appending to a log
 */
void DockLog::log(const QString &message)
{
    ui->textEdit->appendPlainText(message);
    ui->textEdit->ensureCursorVisible();
}

/*
 * Menu is blocked to only support copy and select-all
 */
void DockLog::showContextMenu(const QPoint &pt)
{
    QMenu *menu = new QMenu(this);
    menu->addAction("Select All", ui->textEdit, SLOT(selectAll()), QKeySequence::SelectAll);
    menu->addAction("Copy", ui->textEdit, SLOT(copy()), QKeySequence::Copy);
    menu->addSeparator();
    menu->addAction("Clear", ui->textEdit, SLOT(clear()));
    menu->addAction("Max lines...", this, SLOT(onMaxLines()));

    // These are logging levels:
    // Log level:       3           2             1              0 (can't be disabled)
    // enum QtMsgType   QtDebugMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg
    QSettings settings;
    int logLevel = settings.value("AppLogLevel", 2).toInt();
    if (logLevel==2)
        menu->addAction("Show Trace", this, SLOT(onLogLevel()))->setData(3);
    else
        menu->addAction("Hide Trace", this, SLOT(onLogLevel()))->setData(2);

    menu->exec(ui->textEdit->mapToGlobal(pt));
    delete menu;
}

/*
 * Menu handler to set the max number of lines to keep
 */
void DockLog::onMaxLines()
{
    bool ok;
    // Get and change the number of lines that we keep in the log buffer
    QSettings settings;
    int lines = settings.value("AppLogLines", 2000).toInt();

    lines = QInputDialog::getInt(this, "Set log buffer size", "Enter the number of lines of text to keep in the log buffer [100-5000]:", lines, 100, 5000, 500, &ok);
    if (ok)
    {
        ui->textEdit->setMaximumBlockCount(lines);
        settings.setValue("AppLogLines", lines);
    }
}

/*
 * Changes the log level
 */
void DockLog::onLogLevel()
{
    // Make sure this slot is really called by a context menu action, so it carries the data we need
    if (QAction* contextAction = qobject_cast<QAction*>(sender()))
    {
        const int logLevel = contextAction->data().toInt();
        QSettings settings;
        settings.setValue("AppLogLevel", logLevel);
    }
}
