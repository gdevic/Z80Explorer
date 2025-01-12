#ifndef DIALOGEDITWATCHLIST_H
#define DIALOGEDITWATCHLIST_H

#include "ClassNetlist.h"
#include <QDialog>
class QListWidgetItem;

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

private:
    QListWidgetItem *getListItem(ClassNetlist &Net, QString name);

private slots:
    void onAdd();
    void onAddAll();
    void onRemove();
    void onRemoveAll();
    void allSelChanged();
    void listSelChanged();

private:
    Ui::DialogEditWatchlist *ui;
};

#endif // DIALOGEDITWATCHLIST_H
