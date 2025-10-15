cmake_minimum_required(VERSION 3.19)
project(Z80Explorer LANGUAGES CXX)

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Xml Concurrent Qml Network)

set(CXX_STANDARD C++17)

qt_standard_project_setup()

qt_add_executable(Z80Explorer
    WIN32 MACOSX_BUNDLE
    src/ClassAnnotate.cpp
    src/ClassApplog.cpp
    src/ClassColors.cpp
    src/ClassController.cpp
    src/ClassLogic.cpp
    src/ClassNetlist.cpp
    src/ClassScript.cpp
    src/ClassServer.cpp
    src/ClassSimZ80.cpp
    src/ClassTip.cpp
    src/ClassTrickbox.cpp
    src/ClassVisual.cpp
    src/ClassWatch.cpp
    src/DialogEditAnnotations.cpp
    src/DialogEditBuses.cpp
    src/DialogEditColors.cpp
    src/DialogEditNets.cpp
    src/DialogEditSchematic.cpp
    src/DialogEditWatchlist.cpp
    src/DialogEditWaveform.cpp
    src/DialogSchematic.cpp
    src/DockCommand.cpp
    src/DockImageView.cpp
    src/DockLog.cpp
    src/DockMonitor.cpp
    src/DockWaveform.cpp
    src/MainWindow.cpp
    src/WidgetEditColor.cpp
    src/WidgetGraphicsView.cpp
    src/WidgetHistoryLineEdit.cpp
    src/WidgetImageOverlay.cpp
    src/WidgetImageView.cpp
    src/WidgetToolbar.cpp
    src/WidgetWaveform.cpp
    src/main.cpp
	
    src/AppTypes.h
    src/ClassAnnotate.h
    src/ClassApplog.h
    src/ClassColors.h
    src/ClassController.h
    src/ClassException.h
    src/ClassLogic.h
    src/ClassNetlist.h
    src/ClassScript.h
    src/ClassServer.h
    src/ClassSimZ80.h
    src/ClassSingleton.h
    src/ClassTip.h
    src/ClassTrickbox.h
    src/ClassVisual.h
    src/ClassWatch.h
    src/DialogEditAnnotations.h
    src/DialogEditBuses.h
    src/DialogEditColors.h
    src/DialogEditNets.h
    src/DialogEditSchematic.h
    src/DialogEditWatchlist.h
    src/DialogEditWaveform.h
    src/DialogSchematic.h
    src/DockCommand.h
    src/DockImageView.h
    src/DockLog.h
    src/DockMonitor.h
    src/DockWaveform.h
    src/MainWindow.h
    src/WidgetEditColor.h
    src/WidgetGraphicsView.h
    src/WidgetHistoryLineEdit.h
    src/WidgetImageOverlay.h
    src/WidgetImageView.h
    src/WidgetToolbar.h
    src/WidgetWaveform.h
    src/z80state.h

    src/DialogEditAnnotations.ui
    src/DialogEditBuses.ui
    src/DialogEditColors.ui
    src/DialogEditNets.ui
    src/DialogEditSchematic.ui
    src/DialogEditWatchlist.ui
    src/DialogEditWaveform.ui
    src/DialogSchematic.ui
    src/DockCommand.ui
    src/DockImageView.ui
    src/DockLog.ui
    src/DockMonitor.ui
    src/DockWaveform.ui
    src/MainWindow.ui
    src/WidgetEditColor.ui
    src/WidgetImageOverlay.ui
    src/WidgetToolbar.ui
	
    src/Z80Explorer.rc

    resource/init.js

    .gitignore

    LICENSE.txt
    README.md
)


target_link_libraries(Z80Explorer
    PRIVATE
        Qt::Concurrent 
        Qt::Network		
        Qt::Qml
        Qt::Widgets
        Qt::Xml 
)

target_include_directories(Z80Explorer
    PRIVATE
        src
)

target_compile_definitions(Z80Explorer
    PRIVATE
        QT_DEPRECATED_WARNINGS
)

if (WIN32)
    target_compile_definitions(Z80Explorer
        PRIVATE
            WINDOWS
            _CRT_SECURE_NO_WARNINGS
    )
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set_property(CACHE
        CMAKE_INSTALL_PREFIX PROPERTY VALUE "${CMAKE_CURRENT_BINARY_DIR}/installation"
    )
endif()

if (WIN32)
    set(CMAKE_INSTALL_BINDIR .)
    set(CMAKE_INSTALL_DATADIR .)
    set(CMAKE_INSTALL_DOCDIR docs)
    set(INDEX_HTML DESTINATION .)
    set(MORE_DEPLOY_TOOL_OPTIONS
        --libdir .
        --no-compiler-runtime     # vc_redist.[x64.]exe
        --no-system-d3d-compiler  # d3dcompiler_47.dll
        --no-system-dxc-compiler  # dxcompiler.dll dxil.dll
        --no-opengl-sw            # opengl32sw.dll
    )
elseif (APPLE)
    set(CMAKE_INSTALL_DOCDIR docs)
    set(INDEX_HTML DESTINATION .)
else()
    # Install our stuff in share/Z80Explorer, not in the base folder.
    set(CMAKE_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
    # index.html is not installed on non-Apple Unix systems,
    # since there's no "root" install folder where one could put it.
    unset(INDEX_HTML)
    # Documentation lives in share/doc or wherever it's expected by the distribution's
    # concentions, no need to change that as our documentation files have unique names.
endif()

install(TARGETS Z80Explorer)

qt_generate_deploy_app_script(
    TARGET Z80Explorer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_TOOL_OPTIONS
        --skip-plugin-types bearer,iconengines,imageformats,platforminputcontexts,qmltooling,tls,networkinformation,generic
        --no-translations
        ${MORE_DEPLOY_TOOL_OPTIONS}
)

install(SCRIPT ${deploy_script})

install(FILES
    docs/Z80Explorer.docx
    docs/Z80Explorer.html
    docs/Z80Explorer.pdf
    TYPE DOC
)

if (DEFINED INDEX_HTML)
    install(FILES index.html ${INDEX_HTML})
endif()

install(DIRECTORY
    resource
    TYPE DATA
    PATTERN "layermap.bin" EXCLUDE
)

if (WIN32)
    install(FILES cleanup.reg TYPE DATA)
    install(CODE [=[
        file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/qml")
        file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/bin")
    ]=])
endif()
