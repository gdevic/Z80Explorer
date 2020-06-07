#include "DialogSchematic.h"
#include "ui_DialogSchematic.h"
#include "ClassController.h"
#include <QStringBuilder>

DialogSchematic::DialogSchematic(QWidget *parent, Logic *lr) :
    QDialog(parent),
    ui(new Ui::DialogSchematic),
    m_logic(lr)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false); // We never delete this view, just hide it

    net_t net = lr->net;
    QString name = ::controller.getNetlist().get(net);
    if (name.isEmpty())
        setWindowTitle(QString("Schematic net %1").arg(net));
    else
        setWindowTitle(QString("Schematic net %1 \"%2\"").arg(net).arg(name));
}

DialogSchematic::~DialogSchematic()
{
    delete ui;
}
