@REM Relase script specific to Visual Studio 2019 and Qt 5.15.2
set RELEASEDIR=Z80Explorer
if not exist "Z80Explorer.exe" Goto end
mkdir release
xcopy /Y Z80Explorer.exe release
cd release

@REM call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

set path=C:\Qt\5.15.2\msvc2019_64\bin;%VCINSTALLDIR%;%path%

windeployqt.exe --compiler-runtime Z80Explorer.exe
if errorlevel 1 goto end

mkdir %RELEASEDIR%
mkdir %RELEASEDIR%\platforms
mkdir %RELEASEDIR%\styles

xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vccorlib140.dll    %RELEASEDIR%
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vcruntime140.dll   %RELEASEDIR%
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\vcruntime140_1.dll %RELEASEDIR%
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140.dll       %RELEASEDIR%
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140_1.dll     %RELEASEDIR%
xcopy /Y "%VCToolsRedistDir%"\x64\Microsoft.VC142.CRT\msvcp140_2.dll     %RELEASEDIR%
REM xcopy /Y "%VCToolsRedistDir%"\vcredist_x64.exe %RELEASEDIR%

xcopy /Y Z80Explorer.exe  %RELEASEDIR%
xcopy /Y highDPI.bat      %RELEASEDIR%
xcopy /Y cleanup.reg      %RELEASEDIR%
xcopy /Y Qt5Core.dll      %RELEASEDIR%
xcopy /Y Qt5Gui.dll       %RELEASEDIR%
xcopy /Y Qt5Script.dll    %RELEASEDIR%
xcopy /Y Qt5Network.dll   %RELEASEDIR%
xcopy /Y Qt5Widgets.dll   %RELEASEDIR%
xcopy /Y platforms\qwindows.dll %RELEASEDIR%\platforms
xcopy /Y styles\qwindowsvistastyle.dll %RELEASEDIR%\styles

@echo Download Z80 resources from a public git repo...
cd %RELEASEDIR%
git clone --depth=1 --branch=master https://github.com/gdevic/Z80Explorer_Z80.git resource
rmdir /S /Q "resource/".git"
cd ..
del *.dll Z*.exe
rmdir /S /Q bearer iconengines imageformats platforms styles translations

@echo.
@echo Release files are in Z80Explorer folder
:end
