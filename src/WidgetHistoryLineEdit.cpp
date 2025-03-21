#include "WidgetHistoryLineEdit.h"
#include <QAbstractItemView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTimer>

WidgetHistoryLineEdit::WidgetHistoryLineEdit(QWidget* parent) :
    QLineEdit(parent), m_completer(new QCompleter(this)), m_completionModel(new QStringListModel(this))
{
    // Setup completer
    m_completer->setModel(m_completionModel);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(m_completer);

    connect(this, &QLineEdit::textEdited, this, &WidgetHistoryLineEdit::onTextEdited);
    connect(this, &QLineEdit::returnPressed, this, &WidgetHistoryLineEdit::onReturnPressed);
}

void WidgetHistoryLineEdit::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Escape: // Hitting ESC closes the completer box first, then clears the edit text
        if (m_completer->popup()->isVisible())
            m_completer->popup()->hide();
        else
            clear();
        break;
    case Qt::Key_Up:
        navigateHistory(-1);
        break;
    case Qt::Key_Down:
        navigateHistory(1);
        break;
    case Qt::Key_Delete:
        if (isNavigatingHistory())
        {
            removeCurrentHistoryItem();
            event->accept();
            return;
        }
        [[fallthrough]];
    default:
        QLineEdit::keyPressEvent(event);
    }
}

void WidgetHistoryLineEdit::onReturnPressed()
{
    QString token = text().trimmed();
    addToHistory(token);

    // QCompleter's architecture requires the user to hit Enter twice: once to accept an item and then the
    // second time the edit widget completes the edit. Using the timer solves the problem of the text
    // remaining in the edit box after the first Enter has been processed.
    QTimer::singleShot(0, [=]() { clear(); });

    emit textEntered(token);
}

void WidgetHistoryLineEdit::removeCurrentHistoryItem()
{
    if (!isNavigatingHistory() || m_history.isEmpty())
        return;

    // Calculate the actual index in the history vector
    int actualIndex = m_history.size() - 1 - m_historyPosition;
    if ((actualIndex >= 0) && (actualIndex < m_history.size()))
    {
        // Remove the item
        m_history.remove(actualIndex);

        // Adjust history position if needed
        if (m_history.isEmpty())
        {
            m_historyPosition = -1;
            setText(m_currentText);
        }
        else
        {
            // Stay at the same position unless we removed the last item
            if (m_historyPosition >= m_history.size())
                m_historyPosition--;

            // Update the displayed text
            if (m_historyPosition >= 0)
                setText(m_history[m_history.size() - 1 - m_historyPosition]);
            else
                setText(m_currentText);
        }
    }
}

/*
 * There are two mutually exclusive edit states:
 * 1. User started typing which activates the completer; the up/down keys select items from the completer's list
 * 2. Scrolling up/down through the history; completer is not active
 * When m_historyPosition is -1, the state is (1). Any other value represents an index (from the last
 * element in the m_history[]) that is shown as the history item. The history selection wraps around.
 * In addition, we are keeping the text user has already typed, m_currentText, so it can be retrieved
 * when the history wraps around.
 */
void WidgetHistoryLineEdit::navigateHistory(int direction)
{
    if (m_history.isEmpty())
        return;

    if (m_historyPosition == -1) // If not currently navigating history store the current text
        m_currentText = text();

    int newPosition = m_historyPosition + direction;

    // For indices that should wrap around, we first insert the current text
    if ((newPosition >= m_history.size()) || (newPosition == -1))
    {
        m_historyPosition = -1;
        setText(m_currentText);
    }
    else if (newPosition < -1)
    {
        m_historyPosition = m_history.size() - 1;
        setText(m_history[m_history.size() - 1 - m_historyPosition]);
    }
    else
    {
        m_historyPosition = newPosition;
        setText(m_history[m_history.size() - 1 - m_historyPosition]);
    }
}

void WidgetHistoryLineEdit::addToHistory(const QString& text)
{
    if (!text.isEmpty() && (m_history.isEmpty() || (text != m_history.last())))
    {
        m_history.append(text);
        trimHistory();
    }
    m_historyPosition = -1;
    m_currentText.clear();
}

void WidgetHistoryLineEdit::setMaxHistorySize(int size)
{
    if (size < 0)
        return;
    m_maxHistorySize = size;
    trimHistory();
}

void WidgetHistoryLineEdit::clearHistory()
{
    m_history.clear();
    m_historyPosition = -1;
    m_currentText.clear();
}

void WidgetHistoryLineEdit::trimHistory()
{
    while (m_history.size() > m_maxHistorySize)
        m_history.removeFirst();
}

void WidgetHistoryLineEdit::addCompletionItem(const QString& item)
{
    m_completionItems.insert(item);
    updateCompleter();
}

void WidgetHistoryLineEdit::addCompletionItems(const QStringList& items)
{
    for (const auto& item : items)
        m_completionItems.insert(item);
    updateCompleter();
}

void WidgetHistoryLineEdit::removeCompletionItem(const QString& item)
{
    m_completionItems.remove(item);
    updateCompleter();
}

void WidgetHistoryLineEdit::clearCompletionItems()
{
    m_completionItems.clear();
    updateCompleter();
}

void WidgetHistoryLineEdit::updateCompleter()
{
    QStringList items = QStringList(m_completionItems.begin(), m_completionItems.end());
    std::sort(items.begin(), items.end());
    m_completionModel->setStringList(items);
}

void WidgetHistoryLineEdit::onTextEdited(const QString& text)
{
    if (!text.isEmpty())
        m_completer->complete();
}
