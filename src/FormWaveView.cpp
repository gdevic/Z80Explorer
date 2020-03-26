#include "FormWaveView.h"
#include "ui_FormWaveView.h"
#include "ClassSim.h"

//============================================================================
// Class constructor and destructor
//============================================================================

FormWaveView::FormWaveView(QWidget *parent, ClassSim *sim) :
    QWidget(parent),
    ui(new Ui::FormWaveView),
    m_sim(sim)
{
    ui->setupUi(this);
}

FormWaveView::~FormWaveView()
{
    delete ui;
}
