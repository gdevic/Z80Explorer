#include "DockImageView.h"
#include "ui_DockImageView.h"
#include <QSettings>

DockImageView::DockImageView(QWidget *parent, QString sid) :
    QDockWidget(parent),
    ui(new Ui::DockImageView)
{
    ui->setupUi(this);
    setWhatsThis(sid);
    setWindowTitle("Image View " + sid);
    QSettings settings;
    restoreGeometry(settings.value("dockImageviewGeometry-" + sid).toByteArray());

    ui->pane->init(sid);
}

DockImageView::~DockImageView()
{
    QSettings settings;
    settings.setValue("dockImageviewGeometry-" + whatsThis(), saveGeometry());

    delete ui;
}
