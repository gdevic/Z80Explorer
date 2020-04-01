QT += core gui widgets xml

CONFIG += c++11

INCLUDEPATH += src

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
    src/ClassApplog.cpp \
    src/ClassChip.cpp \
    src/ClassSim.cpp \
    src/ClassSimX.cpp \
    src/ClassWatch.cpp \
    src/DialogEditWatchlist.cpp \
    src/DockCommand.cpp \
    src/DockImageView.cpp \
    src/DockLog.cpp \
    src/DockWaveform.cpp \
    src/MainWindow.cpp \
    src/WidgetImageOverlay.cpp \
    src/WidgetImageView.cpp \
    src/WidgetWaveform.cpp \
    src/Z80_Simulator.cpp \
    src/main.cpp

HEADERS += \
    src/ClassApplog.h \
    src/ClassChip.h \
    src/ClassException.h \
    src/ClassSim.h \
    src/ClassSimX.h \
    src/ClassSingleton.h \
    src/ClassWatch.h \
    src/DialogEditWatchlist.h \
    src/DockCommand.h \
    src/DockImageView.h \
    src/DockLog.h \
    src/DockWaveform.h \
    src/MainWindow.h \
    src/WidgetImageOverlay.h \
    src/WidgetImageView.h \
    src/WidgetWaveform.h \
    src/Z80_Simulator.h

FORMS += \
    src/DialogEditWatchlist.ui \
    src/DockCommand.ui \
    src/DockImageView.ui \
    src/DockLog.ui \
    src/DockWaveform.ui \
    src/MainWindow.ui \
    src/WidgetImageOverlay.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
