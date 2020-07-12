#ifndef DIALOGEDITBUS_H
#define DIALOGEDITBUS_H

#include <QDialog>

namespace Ui { class DialogEditBuses; }

/*
 * This dialog lets the user define buses from individual nets
 */
class DialogEditBuses : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditBuses(QWidget *parent = nullptr);
    ~DialogEditBuses();

private:
    void add(QString busName, QStringList nets);

private slots:
    void onCreate();
    void onDelete();
    void netSelChanged();
    void busSelChanged();
    void accept() override;

private:
    Ui::DialogEditBuses *ui;
};

#endif // DIALOGEDITBUS_H
