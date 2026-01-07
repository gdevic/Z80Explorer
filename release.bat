@REM Release script for Visual Studio 2022 (Community Edition) and Qt 6
@REM Z80Explorer.exe must be precompiled and stored in the root project folder
@REM Run from "Developer command prompt for VS 2022"
@REM Usage: release.bat <Qt-path>
@REM Example: release.bat C:\Qt\6.10.1\msvc2022_64
if "%~1"=="" (
    echo Usage: release.bat ^<Qt-path^>
    echo Example: release.bat C:\Qt\6.10.1\msvc2022_64
    goto end
)
if not exist "%~1\bin\windeployqt.exe" (
    echo ERROR: windeployqt.exe not found in %~1\bin
    goto end
)
if not exist "Z80Explorer.exe" (
    echo ERROR: Z80Explorer.exe not found. Build it first.
    goto end
)
set PATH=%~1\bin;%PATH%

mkdir release
cd release
xcopy /Y ..\Z80Explorer.exe .
xcopy /Y ..\highDPI.bat .
xcopy /Y ..\cleanup.reg .
xcopy /Y ..\index.html .

windeployqt.exe --compiler-runtime Z80Explorer.exe
if errorlevel 1 goto end

xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC143.CRT\vccorlib140.dll    .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC143.CRT\vcruntime140.dll   .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC143.CRT\vcruntime140_1.dll .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC143.CRT\msvcp140.dll       .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC143.CRT\msvcp140_1.dll     .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC143.CRT\msvcp140_2.dll     .

mkdir resource
xcopy /Y /S ..\resource resource

mkdir docs
xcopy /Y /S ..\docs docs

@REM Remove all unnecessary components and files
rmdir /S /Q bearer iconengines imageformats translations platforminputcontexts qml qmltooling tls networkinformation generic
rm -f Qt6Pdf.dll Qt6VirtualKeyboard.dll Qt6Quick3DUtils.dll Qt6Quick.dll Qt6QmlModels.dll Qt6Svg.dll
rm -f Qt6OpenGL.dll opengl32sw.dll dxcompiler.dll d3dcompiler_47.dll dxil.dll
rm -f Qt6QmlWorkerScript.dll Qt6QmlMeta.dll
rm -f Qt6Lottie.dll Qt6LottieVectorImageGenerator.dll Qt6QuickVectorImageGenerator.dll
rm -f vc_redist.x64.exe
rm -f resource\layermap.bin
@echo It is OK if The system cannot find the file specified.

cd ..

@echo.
@echo DONE
:end
