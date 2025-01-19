@REM Relase script specific to Visual Studio 2019 and Qt 6.7.2
@REM Z80Explorer.exe must be precompiled and stored in the root project folder
@REM Run within the "Developer command prompt for VS 2019" (CMD)
if not exist "Z80Explorer.exe" Goto end

set path=C:\Qt\6.7.2\msvc2019_64\bin;%VCINSTALLDIR%;%path%

mkdir release
cd release
xcopy /Y ..\Z80Explorer.exe .
xcopy /Y ..\highDPI.bat .
xcopy /Y ..\cleanup.reg .
xcopy /Y ..\index.html .

windeployqt.exe --compiler-runtime Z80Explorer.exe
if errorlevel 1 goto end

xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vccorlib140.dll    .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vcruntime140.dll   .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vcruntime140_1.dll .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140.dll       .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140_1.dll     .
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140_2.dll     .

mkdir resource
xcopy /Y /S ..\resource resource

mkdir docs
xcopy /Y /S ..\docs docs

@REM Remove all unnecessary components and files
rmdir /S /Q bearer iconengines imageformats translations platforminputcontexts qml qmltooling tls networkinformation generic
rm -f Qt6Pdf.dll Qt6VirtualKeyboard.dll Qt6Quick3DUtils.dll Qt6Quick.dll Qt6QmlModels.dll Qt6Svg.dll
rm -f Qt6OpenGL.dll opengl32sw.dll dxcompiler.dll d3dcompiler_47.dll dxil.dll
rm -f vc_redist.x64.exe
rm -f resource\layermap.bin
@echo It is OK if The system cannot find the file specified.

cd ..

@echo.
@echo DONE
:end
