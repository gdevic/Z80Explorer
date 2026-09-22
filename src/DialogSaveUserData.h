#ifndef DIALOGSAVEUSERDATA_H
#define DIALOGSAVEUSERDATA_H

#include <QDialog>
#include <QStringList>
#include <QVector>

class QCheckBox;
namespace Ui { class DialogSaveUserData; }

/*
 * This dialog writes the user data files to disk at any point in a session, instead of only when
 * the application closes. Each row is one item of the controller's save registry; rows that nothing
 * backs yet, such as a waveform view that is not open, are shown but disabled.
 */
class DialogSaveUserData : public QDialog
{
    Q_OBJECT

public:
    explicit DialogSaveUserData(QWidget *parent = nullptr);
    ~DialogSaveUserData();

private slots:
    void accept() override;             // Saves the checked items and closes on success
    void onAllToggled();                // The master checkbox ticks or clears every available row

private:
    void buildRows();                   // Creates one row per save-registry item
    void applyMinimumHeight();          // Floors the dialog height at what all the rows need
    void syncAll();                     // Recomputes the master checkbox from the individual rows

    Ui::DialogSaveUserData *ui;
    QVector<QCheckBox *> m_checks;      // One per registry item, in registry order; disabled when unavailable
    QStringList m_ids;                  // Registry id behind each checkbox, same order
};

#endif // DIALOGSAVEUSERDATA_H
