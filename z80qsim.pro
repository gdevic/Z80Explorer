QT += core gui widgets xml

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/ClassApplog.cpp \
    src/ClassBaseGraph.cpp \
    src/ClassChip.cpp \
    src/ClassDockCollection.cpp \
    src/CommandWindow.cpp \
    src/FormGraphWindow.cpp \
    src/LogWindow.cpp \
    src/MainWindow.cpp \
    src/main.cpp

HEADERS += \
    src/ClassApplog.h \
    src/ClassBaseGraph.h \
    src/ClassChip.h \
    src/ClassDockCollection.h \
    src/ClassException.h \
    src/ClassSingleton.h \
    src/CommandWindow.h \
    src/FormGraphWindow.h \
    src/LogWindow.h \
    src/MainWindow.h

FORMS += \
    src/CommandWindow.ui \
    src/FormGraphWindow.ui \
    src/LogWindow.ui \
    src/MainWindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
