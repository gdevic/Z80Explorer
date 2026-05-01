cmake_minimum_required(VERSION 3.19)
project(Z80Explorer LANGUAGES CXX)

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Xml Concurrent Qml Network HttpServer)

qt_standard_project_setup()

# ClassSimZ80_AVX2.{cpp,h} require MSVC + AVX2 intrinsics; include them only on Windows.
file(GLOB SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h src/*.ui)
if (NOT WIN32)
    list(FILTER SOURCES EXCLUDE REGEX "ClassSimZ80_AVX2")
endif()

qt_add_executable(Z80Explorer WIN32 MACOSX_BUNDLE ${SOURCES})

target_link_libraries(Z80Explorer PRIVATE
    Qt::Concurrent Qt::HttpServer Qt::Network Qt::Qml Qt::Widgets Qt::Xml)

target_include_directories(Z80Explorer PRIVATE src)
target_compile_definitions(Z80Explorer PRIVATE QT_DEPRECATED_WARNINGS)

if (WIN32)
    target_compile_definitions(Z80Explorer PRIVATE WINDOWS _CRT_SECURE_NO_WARNINGS)
    if (MSVC)
        target_compile_options(Z80Explorer PRIVATE /arch:AVX2 /MP)
    else()
        target_compile_options(Z80Explorer PRIVATE -mavx2)
    endif()
    set(CMAKE_INSTALL_BINDIR .)
    set(CMAKE_INSTALL_DATADIR .)
    set(CMAKE_INSTALL_DOCDIR docs)
    set(INDEX_HTML DESTINATION .)
    set(DEPLOY_TOOL_OPTIONS
        --skip-plugin-types bearer,iconengines,imageformats,platforminputcontexts,qmltooling,tls,networkinformation,generic
        --no-translations
        --libdir .
        --no-compiler-runtime     # don't bundle the 25 MB vc_redist.x64.exe; we ship the DLLs ourselves below
        --no-system-d3d-compiler  # d3dcompiler_47.dll
        --no-system-dxc-compiler  # dxcompiler.dll dxil.dll
        --no-opengl-sw            # opengl32sw.dll
    )
    # Bundle the MSVC runtime DLLs (vcruntime140.dll, msvcp140.dll, ...) next to the .exe so the zip
    # runs on machines without VC++ Redistributable installed. Smaller than --compiler-runtime, which
    # would copy the 25 MB vc_redist.x64.exe installer instead of the side-by-side DLLs.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    include(InstallRequiredSystemLibraries)
elseif (APPLE)
    # Install everything inside the .app bundle so the DMG (built from just the .app) is self-contained.
    set(CMAKE_INSTALL_DATADIR Z80Explorer.app/Contents/Resources)
    set(CMAKE_INSTALL_DOCDIR  Z80Explorer.app/Contents/Resources/docs)
    set(INDEX_HTML DESTINATION Z80Explorer.app/Contents/Resources)
else()
    # Linux: flat portable bundle. The user is expected to extract the tarball and run the binary
    # from the bundle root, so resource/ must sit next to the executable. Qt libs still go under lib/
    # (CMAKE_INSTALL_LIBDIR default); qt_generate_deploy_app_script computes the right RPATH.
    set(CMAKE_INSTALL_BINDIR .)
    set(CMAKE_INSTALL_DATADIR .)
    set(CMAKE_INSTALL_DOCDIR docs)
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "${CMAKE_CURRENT_BINARY_DIR}/installation")
endif()

install(TARGETS Z80Explorer BUNDLE DESTINATION .)

qt_generate_deploy_app_script(
    TARGET Z80Explorer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_TOOL_OPTIONS ${DEPLOY_TOOL_OPTIONS})
install(SCRIPT ${deploy_script})

install(FILES docs/Z80Explorer.docx docs/Z80Explorer.html docs/Z80Explorer.pdf TYPE DOC)

if (DEFINED INDEX_HTML)
    install(FILES index.html ${INDEX_HTML})
endif()

install(DIRECTORY resource TYPE DATA PATTERN "layermap.bin" EXCLUDE)

if (WIN32)
    install(FILES cleanup.reg TYPE DATA)
    install(CODE [=[
        file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/qml")
        file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/bin")
    ]=])
endif()
