#ifndef WIDGETHISTORYLINEEDIT_H
#define WIDGETHISTORYLINEEDIT_H

#include <QLineEdit>
#include <QCompleter>
#include <QSet>
#include <QStringListModel>

/*
 * This class extends QLineEdit with history and completion lists
 *
 * The completion list can be set at any time and opens as the user starts typing text. The match heuristic
 * can be selected in the class constructor (currently, it is "match any").
 * The history is supported by up/down keys.
 * DEL key removes the selected history item from the history list.
 * ESC key closes the completer and then clears the input text.
 */
class WidgetHistoryLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit WidgetHistoryLineEdit(QWidget* parent);
    ~WidgetHistoryLineEdit() {};

    // History management
    void addToHistory(const QString& text);
    void clearHistory();
    QStringList history() const { return m_history.toList(); }

    // Completion management
    void addCompletionItem(const QString& item);
    void addCompletionItems(const QStringList& items);
    void removeCompletionItem(const QString& item);
    void clearCompletionItems();
    QStringList completionItems() const { return QStringList(m_completionItems.begin(), m_completionItems.end()); }

    // Settings
    int maxHistorySize() const { return m_maxHistorySize; }
    void setMaxHistorySize(int size);

signals:
    void textEntered(QString text);                // New text entered in the "Find" edit box

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onTextEdited(const QString& text);
    void onReturnPressed();

private:
    bool isNavigatingHistory() const { return m_historyPosition != -1; }
    int currentHistoryIndex() const { return m_historyPosition; }
    void removeCurrentHistoryItem();

    void navigateHistory(int direction);
    void updateCompleter();
    void trimHistory();

    QVector<QString> m_history;
    int m_maxHistorySize {10};
    int m_historyPosition {-1};
    QString m_currentText;

    QSet<QString> m_completionItems;
    QCompleter* m_completer;
    QStringListModel* m_completionModel;
};

#endif // WIDGETHISTORYLINEEDIT_H
