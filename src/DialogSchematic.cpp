#include "DialogSchematic.h"
#include "ui_DialogSchematic.h"
#include "ClassController.h"
#include <QStringBuilder>

DialogSchematic::DialogSchematic(QWidget *parent, net_t net) :
    QDialog(parent),
    ui(new Ui::DialogSchematic),
    m_net(net)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false); // We never delete this view, just hide it
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
