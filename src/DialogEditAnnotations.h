#ifndef DIALOGEDITANNOTATIONS_H
#define DIALOGEDITANNOTATIONS_H

#include <QDialog>

namespace Ui { class DialogEditAnnotations; }

/*
 * This class contains the UI and the code to edit annotations
 */
class DialogEditAnnotations : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditAnnotations(QWidget *parent = nullptr);
    ~DialogEditAnnotations();

private:
    Ui::DialogEditAnnotations *ui;
};

#endif // DIALOGEDITANNOTATIONS_H
