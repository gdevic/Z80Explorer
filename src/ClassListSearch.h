#ifndef CLASSLISTSEARCH_H
#define CLASSLISTSEARCH_H

#include <QObject>

class QLineEdit;
class QListWidget;

/*
 * Wires a QLineEdit to a QListWidget so that typing in the edit
 * filters the list down to items whose visible text contains the
 * typed substring (case-insensitive). The non-matching items are
 * hidden, not removed - the underlying list is preserved, sortingEnabled
 * keeps working, and item data / tooltips set by callers are untouched.
 *
 * After every text change the first still-visible row is selected so
 * that arrow keys + Enter operate on it without an extra click.
 *
 * Use ClassListSearch::install(edit, list) once per pair, after
 * ui->setupUi(this). The helper attaches itself as a child of the
 * list widget and lives for the dialog's lifetime.
 */
class ClassListSearch : public QObject
{
    Q_OBJECT
public:
    static void install(QLineEdit *edit, QListWidget *list);

private:
    explicit ClassListSearch(QLineEdit *edit, QListWidget *list);
    void onTextChanged(const QString &text);

    QLineEdit   *m_edit;
    QListWidget *m_list;
};

#endif // CLASSLISTSEARCH_H
