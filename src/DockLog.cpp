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
    connect(ui->textEdit, &QLineEdit::customContextMenuRequested, this, &DockLog::showContextMenu);

    // Connect log message slot
    CAppLogHandler *applog = &Singleton<CAppLogHandler>::Instance();
    connect(applog, &CAppLogHandler::NewLogMessage, this, &DockLog::processNewMessage);
}

DockLog::~DockLog()
{
    delete ui;
}

/*
 * Slot functions that receives log messages and display them in the log window
 */
void DockLog::processNewMessage(QString message, bool)
{
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
    // Log level:       4           3          2,            1              0 (can't be disabled)
    // enum QtMsgType   QtDebugMsg, QtInfoMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg
    QSettings settings;
    uint logLevel = settings.value("logLevel", 3).toUInt();
    uint logOptions = settings.value("logOptions").toUInt();

    // Create and handle menu to show or hide debug messages
    QAction *actionShowDebug = new QAction("Show Debug", this);
    actionShowDebug->setCheckable(true);
    actionShowDebug->setChecked(logLevel == 4);
    menu->addAction(actionShowDebug);

    connect(actionShowDebug, &QAction::triggered, this, [](bool checked)
    {
        QSettings settings;
        settings.setValue("logLevel", checked ? 4 : 3);
    });

    // Create and handle menu to log to a file or not to log to a file
    QAction *actionLogToFile = new QAction("Log to file", this);
    actionLogToFile->setCheckable(true);
    actionLogToFile->setChecked(logOptions & LogOptions_File);
    menu->addAction(actionLogToFile);

    connect(actionLogToFile, &QAction::triggered, this, [](bool checked)
    {
        CAppLogHandler *applog = &Singleton<CAppLogHandler>::Instance();
        uint logOptions = applog->GetLogOptions() & ~LogOptions_File;
        if (checked)
            logOptions |= LogOptions_File;
        applog->SetLogOptions(logOptions);

        QSettings settings;
        settings.setValue("logOptions", logOptions);
    });

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

    lines = QInputDialog::getInt(this, "Set log size", "Enter the number of lines to keep [100-5000]:", lines, 100, 5000, 500, &ok);
    if (ok)
    {
        ui->textEdit->setMaximumBlockCount(lines);
        settings.setValue("AppLogLines", lines);
    }
}
