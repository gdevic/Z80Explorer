#ifndef DOCKIMAGEVIEW_H
#define DOCKIMAGEVIEW_H

#include <QDockWidget>

namespace Ui { class DockImageView; }

/*
 * ClassDockImageView is a dockable container to show the chip image view
 */
class DockImageView : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockImageView(QWidget *parent, uint id);
    ~DockImageView();

private:
    Ui::DockImageView *ui;
};

#endif // DOCKIMAGEVIEW_H
