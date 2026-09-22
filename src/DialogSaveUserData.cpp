#include "ClassController.h"
#include "DialogSaveUserData.h"
#include "ui_DialogSaveUserData.h"
#include <QCheckBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QVBoxLayout>

// The checked rows are remembered between invocations so that a user who only ever saves a subset
// does not have to re-pick it every time. An absent key means "everything", which is the default.
static const char *kSelectionKey = "saveUserDataSelection";

DialogSaveUserData::DialogSaveUserData(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogSaveUserData)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Save)->setDefault(true);

    buildRows();
    syncAll();

    // The rows are built at run time, so how tall the dialog needs to be is only known now. Make
    // that height the floor: the user can enlarge it, but not shrink it to where rows are hidden.
    // Applied before the stored geometry, which Qt then clamps to it.
    applyMinimumHeight();

    QSettings settings;
    if (!restoreGeometry(settings.value("saveUserDataGeometry").toByteArray()))
        resize(width(), minimumHeight()); // First run: open exactly as tall as the rows need

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &DialogSaveUserData::accept);
    // clicked() rather than toggled(), so that syncAll() setting the state back does not recurse
    connect(ui->cbAll, &QCheckBox::clicked, this, &DialogSaveUserData::onAllToggled);
}

DialogSaveUserData::~DialogSaveUserData()
{
    QSettings settings;
    settings.setValue("saveUserDataGeometry", saveGeometry());

    delete ui;
}

/*
 * Creates one checkbox per registry item, labelled with the display name and the files it would
 * write. Paths are shown relative to the resource directory, which is where all of them live and
 * is what makes the difference between chip/ and user/ readable.
 */
void DialogSaveUserData::buildRows()
{
    QSettings settings;
    QDir resDir(settings.value("ResourceDir").toString());
    const bool hadSelection = settings.contains(kSelectionKey);
    const QStringList selected = settings.value(kSelectionKey).toStringList();

    QVBoxLayout *layout = ui->rowsLayout;
    for (const ClassController::SaveItem &item : ::controller.saveItems())
    {
        const bool available = bool(item.save);

        QStringList shown;
        if (item.files)
        {
            const QStringList files = item.files();
            for (const QString &f : files)
                shown.append(resDir.relativeFilePath(f));
        }

        // A checkbox reads '&' as a mnemonic marker, so it has to be doubled to survive on screen.
        // The registry keeps the plain name, which is what the script and MCP surfaces report.
        QString label = QString("%1: %2").arg(item.name, available ? shown.join(" + ") : QString("not open"));
        label.replace(QChar('&'), QStringLiteral("&&"));

        QCheckBox *check = new QCheckBox(label, ui->rows);
        check->setEnabled(available);
        check->setChecked(available && (!hadSelection || selected.contains(item.id)));

        layout->addWidget(check);

        connect(check, &QCheckBox::toggled, this, [this]() { syncAll(); });

        m_checks.append(check);
        m_ids.append(item.id);
    }
    layout->addStretch(1);
}

/*
 * Sets the dialog's minimum height from the rows it just built, so every item stays visible however
 * many the registry holds. A QScrollArea does not grow its own size hint to fit what it holds, so
 * the requirement has to be pushed up: first onto the scroll area, then onto the dialog through the
 * layout. Width is left free, since only the number of rows varies.
 */
void DialogSaveUserData::applyMinimumHeight()
{
    // Room for the horizontal scrollbar too: the width is the user's to shrink, and when it does
    // appear it must not cover the last row
    const int chrome = (ui->scrollArea->frameWidth() * 2) + ui->scrollArea->horizontalScrollBar()->sizeHint().height();
    ui->scrollArea->setMinimumHeight(ui->rows->sizeHint().height() + chrome);
    setMinimumHeight(minimumSizeHint().height());
}

/*
 * Recomputes the master checkbox: ticked when every available row is, cleared when none is, and
 * partially ticked in between.
 */
void DialogSaveUserData::syncAll()
{
    int available = 0, checked = 0;
    for (QCheckBox *check : std::as_const(m_checks))
    {
        if (!check->isEnabled())
            continue;
        available++;
        if (check->isChecked())
            checked++;
    }
    QSignalBlocker block(ui->cbAll);
    ui->cbAll->setEnabled(available > 0);
    ui->cbAll->setCheckState((checked == 0) ? Qt::Unchecked
                           : (checked == available) ? Qt::Checked : Qt::PartiallyChecked);
}

/*
 * The master checkbox only ever means all or nothing; the partial state is something syncAll()
 * reports, never something the user selects.
 */
void DialogSaveUserData::onAllToggled()
{
    const bool on = (ui->cbAll->checkState() != Qt::Unchecked);
    for (QCheckBox *check : std::as_const(m_checks))
    {
        if (check->isEnabled())
            check->setChecked(on);
    }
    syncAll();
}

/*
 * Saves the checked items and reports the outcome. On success the dialog closes; a failure keeps it
 * open with the reason, since the user will want to do something about it.
 */
void DialogSaveUserData::accept()
{
    // Remember the choice. Rows that are unavailable right now keep whatever the stored selection
    // said about them: they were force-unchecked when built, and dropping them would silently
    // forget a choice the user never revisited.
    QSettings settings;
    QStringList remembered = settings.value(kSelectionKey).toStringList();
    QStringList ids;
    for (int i = 0; i < m_checks.count(); i++)
    {
        if (!m_checks.at(i)->isEnabled())
            continue;
        remembered.removeAll(m_ids.at(i));
        if (m_checks.at(i)->isChecked())
        {
            remembered.append(m_ids.at(i));
            ids.append(m_ids.at(i));
        }
    }
    // An empty selection is not worth remembering: storing it would leave the dialog blank on every
    // later visit, with nothing to save until the user re-ticked the rows by hand
    if (!remembered.isEmpty())
        settings.setValue(kSelectionKey, remembered);

    if (ids.isEmpty())
    {
        QMessageBox::information(this, "Save User Data", "Nothing is ticked, so there is nothing to save.");
        return;
    }

    QStringList failed, skipped;
    int files = 0;
    for (const ClassController::SaveResult &r : ::controller.save(ids))
    {
        if (r.outcome == ClassController::SaveWritten)
            files += int(r.files.count());
        else if (r.outcome == ClassController::SaveSkipped)
            skipped.append(QString("%1: %2").arg(r.id, r.reason));
        else
            failed.append(QString("%1: %2").arg(r.id, r.reason));
    }

    if (!failed.isEmpty())
    {
        QMessageBox::critical(this, "Save User Data", QString("Some data could not be saved:\n\n") + failed.join("\n"));
        return;
    }
    // A skip wrote nothing on purpose. Say so rather than counting it as saved, which would tell
    // the user their data is on disk when it is not.
    if (!skipped.isEmpty())
    {
        QMessageBox::information(this, "Save User Data",
            QString("Saved %1 file(s).\n\nNot written:\n\n").arg(files) + skipped.join("\n"));
    }
    qInfo() << "Saved" << files << "user data file(s)," << skipped.count() << "skipped";
    QDialog::accept();
}
