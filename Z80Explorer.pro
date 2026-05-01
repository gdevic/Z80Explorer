QT += core gui widgets xml concurrent qml network httpserver
CONFIG += c++17

INCLUDEPATH += src
RC_ICONS = src/app.ico

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += $$files(src/*.cpp)
HEADERS += $$files(src/*.h)
FORMS   += $$files(src/*.ui)

# ClassSimZ80_AVX2.{cpp,h} require MSVC + AVX2 intrinsics; include them only on Windows.
win32 {
    DEFINES += WINDOWS _CRT_SECURE_NO_WARNINGS
    QMAKE_CXXFLAGS += /arch:AVX2
} else {
    SOURCES -= src/ClassSimZ80_AVX2.cpp
    HEADERS -= src/ClassSimZ80_AVX2.h
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
