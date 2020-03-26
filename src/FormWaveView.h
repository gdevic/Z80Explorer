#ifndef FORMWAVEVIEW_H
#define FORMWAVEVIEW_H

#include <QWidget>

class ClassSim;

namespace Ui { class FormWaveView; }

/*
 * This class implements a window to watch simulation waveforms.
 * It is tied to ClassSim to interface with the simulator.
 */
class FormWaveView : public QWidget
{
    Q_OBJECT

public:
    explicit FormWaveView(QWidget *parent, ClassSim *sim);
    ~FormWaveView();

private:
    Ui::FormWaveView *ui;
    ClassSim *m_sim;
};

#endif // FORMWAVEVIEW_H
