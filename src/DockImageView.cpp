#include "DockImageView.h"
#include "ui_DockImageView.h"
#include <QSettings>

DockImageView::DockImageView(QWidget *parent, uint id) :
    QDockWidget(parent),
    ui(new Ui::DockImageView),
    m_id(id)
{
    ui->setupUi(this);
    setWindowTitle("Image View " + QString::number(m_id));
    QSettings settings;
    restoreGeometry(settings.value("dockImageviewGeometry-" + QString::number(m_id)).toByteArray());

    ui->pane->init();
}

DockImageView::~DockImageView()
{
    QSettings settings;
    settings.setValue("dockImageviewGeometry-" + QString::number(m_id), saveGeometry());

    delete ui;
}
