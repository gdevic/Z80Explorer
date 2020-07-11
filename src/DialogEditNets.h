#ifndef DIALOGEDITNETS_H
#define DIALOGEDITNETS_H

#include <QDialog>

namespace Ui { class DialogEditNets; }

class DialogEditNets : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditNets(QWidget *parent = nullptr);
    ~DialogEditNets();

private:
    Ui::DialogEditNets *ui;
};

#endif // DIALOGEDITNETS_H
