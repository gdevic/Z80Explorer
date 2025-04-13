QT += core gui widgets xml concurrent qml network

CONFIG += c++11

INCLUDEPATH += src

RC_ICONS = src/app.ico

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
win32 {
DEFINES += WINDOWS
DEFINES += _CRT_SECURE_NO_WARNINGS
}

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/ClassAnnotate.cpp \
    src/ClassApplog.cpp \
    src/ClassColors.cpp \
    src/ClassController.cpp \
    src/ClassLogic.cpp \
    src/ClassNetlist.cpp \
    src/ClassScript.cpp \
    src/ClassSimZ80.cpp \
    src/ClassTip.cpp \
    src/ClassTrickbox.cpp \
    src/ClassVisual.cpp \
    src/ClassWatch.cpp \
    src/DialogEditAnnotations.cpp \
    src/DialogEditBuses.cpp \
    src/DialogEditColors.cpp \
    src/DialogEditNets.cpp \
    src/DialogEditSchematic.cpp \
    src/DialogEditWatchlist.cpp \
    src/DialogEditWaveform.cpp \
    src/DialogSchematic.cpp \
    src/DockCommand.cpp \
    src/DockImageView.cpp \
    src/DockLog.cpp \
    src/DockMonitor.cpp \
    src/DockWaveform.cpp \
    src/MainWindow.cpp \
    src/WidgetEditColor.cpp \
    src/WidgetGraphicsView.cpp \
    src/WidgetHistoryLineEdit.cpp \
    src/WidgetImageOverlay.cpp \
    src/WidgetImageView.cpp \
    src/WidgetToolbar.cpp \
    src/WidgetWaveform.cpp \
    src/main.cpp

HEADERS += \
    src/AppTypes.h \
    src/ClassAnnotate.h \
    src/ClassApplog.h \
    src/ClassColors.h \
    src/ClassController.h \
    src/ClassException.h \
    src/ClassLogic.h \
    src/ClassNetlist.h \
    src/ClassScript.h \
    src/ClassSimZ80.h \
    src/ClassSingleton.h \
    src/ClassTip.h \
    src/ClassTrickbox.h \
    src/ClassVisual.h \
    src/ClassWatch.h \
    src/DialogEditAnnotations.h \
    src/DialogEditBuses.h \
    src/DialogEditColors.h \
    src/DialogEditNets.h \
    src/DialogEditSchematic.h \
    src/DialogEditWatchlist.h \
    src/DialogEditWaveform.h \
    src/DialogSchematic.h \
    src/DockCommand.h \
    src/DockImageView.h \
    src/DockLog.h \
    src/DockMonitor.h \
    src/DockWaveform.h \
    src/MainWindow.h \
    src/WidgetEditColor.h \
    src/WidgetGraphicsView.h \
    src/WidgetHistoryLineEdit.h \
    src/WidgetImageOverlay.h \
    src/WidgetImageView.h \
    src/WidgetToolbar.h \
    src/WidgetWaveform.h \
    src/z80state.h

FORMS += \
    src/DialogEditAnnotations.ui \
    src/DialogEditBuses.ui \
    src/DialogEditColors.ui \
    src/DialogEditNets.ui \
    src/DialogEditSchematic.ui \
    src/DialogEditWatchlist.ui \
    src/DialogEditWaveform.ui \
    src/DialogSchematic.ui \
    src/DockCommand.ui \
    src/DockImageView.ui \
    src/DockLog.ui \
    src/DockMonitor.ui \
    src/DockWaveform.ui \
    src/MainWindow.ui \
    src/WidgetEditColor.ui \
    src/WidgetImageOverlay.ui \
    src/WidgetToolbar.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
