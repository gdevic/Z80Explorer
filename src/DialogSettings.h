#ifndef DIALOGSETTINGS_H
#define DIALOGSETTINGS_H

#include <QDialog>

namespace Ui { class DialogSettings; }

/*
 * App-wide settings dialog. Edits the runtime-configurable values:
 * waveform history depth, command socket server enable + port, MCP server enable + port.
 * On accept the values are persisted via QSettings; the controller is then asked to reconcile
 * the running servers, and the new history depth takes effect on the next chip reset.
 */
class DialogSettings : public QDialog
{
    Q_OBJECT

public:
    explicit DialogSettings(QWidget *parent = nullptr);
    ~DialogSettings();

private slots:
    void accept() override;

private:
    Ui::DialogSettings *ui;
};

#endif // DIALOGSETTINGS_H
