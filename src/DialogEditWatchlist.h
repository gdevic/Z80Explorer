#ifndef DIALOGEDITWATCHLIST_H
#define DIALOGEDITWATCHLIST_H

#include <QDialog>

namespace Ui { class DialogEditWatchlist; }

/*
 * This dialog lets the user add or remove nets that are to be watched
 */
class DialogEditWatchlist : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditWatchlist(QWidget *parent);
    ~DialogEditWatchlist();

    void setNodeList(QStringList nodeList);
    void setWatchlist(QStringList nodeList);
    QStringList getWatchlist();

private slots:
    void onAdd();
    void onRemove();

private:
    Ui::DialogEditWatchlist *ui;
};

#endif // DIALOGEDITWATCHLIST_H
