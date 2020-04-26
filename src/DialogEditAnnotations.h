#ifndef DIALOGEDITANNOTATIONS_H
#define DIALOGEDITANNOTATIONS_H

#include "ClassAnnotate.h"
#include <QDialog>
#include <QListWidgetItem>

namespace Ui { class DialogEditAnnotations; }

/*
 * This class contains the UI and the code to edit annotations
 */
class DialogEditAnnotations : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditAnnotations(QWidget *parent);
    ~DialogEditAnnotations();
    void selectRow(int i); // Focus on the selected index

private slots:
    void onUp();
    void onDown();
    void onAdd();
    void onDuplicate();
    void onDelete();
    void selChanged();
    void onTextChanged();
    void onSizeChanged();
    void onXChanged();
    void onYChanged();
    void onBarChanged();
    void onApply();
    void accept() override;
    void reject() override;

private:
    Ui::DialogEditAnnotations *ui;

private:
    // Current working annotation data is kept in each list widget item as a QVariant field data()
    annotation get(QListWidgetItem *item);
    // Original annotation list that is injected if a user clicks on the Cancel button
    QVector<annotation> m_orig;
    void set(QListWidgetItem *item, annotation &annot);
    void append(annotation &annot);
};

#endif // DIALOGEDITANNOTATIONS_H
