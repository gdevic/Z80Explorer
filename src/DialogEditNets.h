#ifndef DIALOGEDITNETS_H
#define DIALOGEDITNETS_H

#include <QDialog>

namespace Ui { class DialogEditNets; }

/*
 * This dialog lets the user rename and delete net names
 */
class DialogEditNets : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditNets(QWidget *parent = nullptr);
    ~DialogEditNets();

private slots:
    void onDelete();
    void onRename();
    void netSelChanged();

private:
    Ui::DialogEditNets *ui;
};

#endif // DIALOGEDITNETS_H
