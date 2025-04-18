#include "ClassController.h"
#include "DialogEditSchematic.h"
#include "DialogSchematic.h"
#include "ui_DialogSchematic.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStringBuilder>
#include <QTimer>

DialogSchematic::DialogSchematic(QWidget *parent, Logic *lr) :
    QDialog(parent),
    ui(new Ui::DialogSchematic),
    m_logic(lr)
{
    ui->setupUi(this);

    net_t net = lr->outnet;
    QString name = ::controller.getNetlist().get(net);
    if (name.isEmpty())
        setWindowTitle(QString("Schematic for net %1").arg(net));
    else
        setWindowTitle(QString("Schematic for net %1:%2").arg(name).arg(net));

    m_scene = new QGraphicsScene(this);

    // Build and connect the context menu
    m_menu->addAction(m_actionShow);
    m_menu->addAction(m_actionNew);
    m_menu->addAction(m_actionPng);
    m_menu->addAction(m_actionSettings);

    connect(m_actionShow, &QAction::triggered, this, &DialogSchematic::onShow);
    connect(m_actionNew, &QAction::triggered, this, &DialogSchematic::onNewSchematic);
    connect(m_actionPng, &QAction::triggered, this, &DialogSchematic::onPng);
    connect(m_actionSettings, &QAction::triggered, this, &DialogSchematic::onSettings);

    ui->view->m_menu = m_menu;

    QTimer::singleShot(0, this, &DialogSchematic::createDrawing);
}

DialogSchematic::~DialogSchematic()
{
    delete ui;
}

void DialogSchematic::onShow()
{
    SymbolItem *item = qgraphicsitem_cast<SymbolItem *>(m_scene->selectedItems().first());
    Q_ASSERT(item);
    emit doShow(item->get());
}

void DialogSchematic::onNewSchematic()
{
    SymbolItem *item = qgraphicsitem_cast<SymbolItem *>(m_scene->selectedItems().first());
    Q_ASSERT(item);
    emit doNewSchematic(item->getNet());
}

void DialogSchematic::onPng()
{
    m_scene->clearSelection();
    QImage image(m_scene->sceneRect().size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    m_scene->render(&painter);

    QString fileName = QFileDialog::getSaveFileName(this, "Save diagram as image", "", "PNG file (*.png);;All files (*.*)");
    if (!fileName.isEmpty() && !image.save(fileName))
        QMessageBox::critical(this, "Error", "Unable to save image file " + fileName);
}

void DialogSchematic::onSettings()
{
    DialogEditSchematic dlg(this);
    dlg.exec();
}

/*
 * Recursively draws symbols and connecting lines of a logic tree structure
 */
void DialogSchematic::drawSymbol(QPoint loc, Logic *lr)
{
    SymbolItem *sym = new SymbolItem(lr, m_actionShow);
    m_scene->addItem(sym);
    sym->setPos(loc);

    QPoint childLoc(loc.x() + 50 + 10, loc.y());
    QPoint lineStart(loc.x() + 50 + 5, loc.y());
    QLine line(lineStart, childLoc);
    int lastY {};

    for (auto k : lr->inputs)
    {
        drawSymbol(childLoc, k);

        m_scene->addLine(line);
        lastY = childLoc.y();
        QPoint nextDown(0, k->tag * 54); // Add a few pixels of vertical spacing
        childLoc += nextDown;
        line.translate(nextDown);
    }

    // Draw a horizontal line from the end of this node to its first input
    if (lr->inputs.count())
        m_scene->addLine(loc.x() + 50, loc.y(), lineStart.x(), loc.y());

    // Draw a vertical line connecting all of its inputs
    if (lr->inputs.count() > 1)
        m_scene->addLine(QLine(lineStart, QPoint(lineStart.x(), lastY)));
}

/*
 * Pre-builds the nodes to calculate screen positions by adding the number of input nodes
 */
int DialogSchematic::preBuild(Logic *lr)
{
    if (lr->inputs.count() == 0)
        lr->tag = 1;
    for (auto k : lr->inputs)
        lr->tag += preBuild(k);
    return lr->tag;
}

void DialogSchematic::createDrawing()
{
    preBuild(m_logic);
    drawSymbol(QPoint(0, 0), m_logic);
    ui->view->setScene(m_scene);
    // Add some margins to the scene view
    m_scene->setSceneRect(m_scene->sceneRect().adjusted(-50, -50, 50, 50));
    ui->view->ensureVisible(QRect());
}
