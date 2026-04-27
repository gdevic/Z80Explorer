#include "ClassListSearch.h"
#include <QLineEdit>
#include <QListWidget>

void ClassListSearch::install(QLineEdit *edit, QListWidget *list)
{
    if (!edit || !list)
        return;
    edit->setClearButtonEnabled(true);
    if (edit->placeholderText().isEmpty())
        edit->setPlaceholderText(QStringLiteral("Search..."));
    new ClassListSearch(edit, list);
}

ClassListSearch::ClassListSearch(QLineEdit *edit, QListWidget *list) :
    QObject(list),
    m_edit(edit),
    m_list(list)
{
    connect(edit, &QLineEdit::textChanged, this, &ClassListSearch::onTextChanged);
}

void ClassListSearch::onTextChanged(const QString &text)
{
    const int n = m_list->count();
    int firstVisible = -1;

    if (text.isEmpty())
    {
        for (int i = 0; i < n; ++i)
            m_list->item(i)->setHidden(false);
        if (n > 0) firstVisible = 0;
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            QListWidgetItem *it = m_list->item(i);
            const bool match = it->text().contains(text, Qt::CaseInsensitive);
            it->setHidden(!match);
            if (match && firstVisible < 0)
                firstVisible = i;
        }
    }

    if (firstVisible >= 0)
        m_list->setCurrentRow(firstVisible);
    else
        m_list->setCurrentRow(-1);
}
